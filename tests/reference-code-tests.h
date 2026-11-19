#include <vector>

struct RelocatedGuard { uint32_t address, size, hash; };
static constexpr RelocatedGuard relocated_guards[] = {
        {0xcaa7b0,741,0x99723fdf},
        {0xc9e950,148,0x91c2da11},
        {0xbdb9d0,165,0xf057ae19},
        {0xb886e0,470,0x64ce6c2f},
    {0xb87410,48,0xa763fd81},
    {0xb87cea,10,0xe8215410},
    {0xac68a0,171,0x0ff6a7f4},
    {0xac6510,912,0xe95ead5c},
    {0xac53c0,10,0x0aabdfd2},
    {0xb3d390,6,0x32e5eb8f},
    {0x1565b54,20,0x83747623},
    {0xc414c0,1943,0xe2ee0198},
    {0xb8d2e0,263,0x2ed294c4},
    {0xb8cfd4,767,0x8f586850},
    {0xbeb710, 270, 0xdf53aaa9}, {0xbeb310, 1024, 0x21f45f84},
    {0xd57b00, 719, 0xdd9fbf3b}, {0xafdf60, 955, 0x91eccdd3},
    {0xca35a0,587,0x82439220}, {0xc43fa0,218,0xca990f65},
    {0xc43f40,96,0x285647ac}, {0xc9ea20,5,0x41be670b},
    {0xc9ee70,21,0x5925ffa9}, {0xc41e80,32,0xaace0ab2},
    {0xfa0d40,12,0x5f095bfe}, {0x967d90,4,0x51abeecc},
};

static void apply_reference_relocations(unsigned char* code,
    const std::vector<uint16_t>& offsets, uint32_t delta)
{
    for (auto offset : offsets) {
        uint32_t word; memcpy(&word, code + offset, sizeof word);
        word += delta; memcpy(code + offset, &word, sizeof word);
    }
}

static void prepare_relocated_reference_image(unsigned char* image, bool exhaustive)
{
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
    const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    assert(directory.VirtualAddress && directory.Size >= 8);
    assert(directory.VirtualAddress < kSteamOctober2024ImageSize &&
        directory.Size <= kSteamOctober2024ImageSize - directory.VirtualAddress);
    unsigned normalized_words = 0;
    for (const auto& guard : relocated_guards) {
        std::vector<uint16_t> offsets;
        // Independent oracle: use the PE file's relocation records, not the
        // production allowlist, to reproduce Windows loader behavior.
        for (uint32_t cursor = 0; cursor < directory.Size;) {
            const auto* block = reinterpret_cast<IMAGE_BASE_RELOCATION*>(image + directory.VirtualAddress + cursor);
            assert(directory.Size - cursor >= 8 && block->SizeOfBlock >= 8 &&
                block->SizeOfBlock <= directory.Size - cursor && !(block->SizeOfBlock & 1));
            const auto* entries = reinterpret_cast<const WORD*>(block + 1);
            for (uint32_t i = 0; i < (block->SizeOfBlock - 8) / 2; ++i) {
                const auto address = kReferenceBase + block->VirtualAddress + (entries[i] & 4095);
                if ((entries[i] >> 12) == IMAGE_REL_BASED_HIGHLOW &&
                    address >= guard.address && address < guard.address + guard.size) {
                    assert(address + 4 <= guard.address + guard.size);
                    offsets.push_back(uint16_t(address - guard.address));
                }
            }
            cursor += block->SizeOfBlock;
        }
        const auto allowlist = Native::CodeRelocations(guard.address);
        assert(offsets == std::vector<uint16_t>(allowlist.offsets, allowlist.offsets + allowlist.count));
        normalized_words += unsigned(offsets.size());
        auto* code = image + guard.address - kReferenceBase;
        if (exhaustive) {
            const std::vector<unsigned char> original(code, code + guard.size);
            for (uint32_t loaded_base : {0x400000u, 0xe0000u, 0x60000000u}) {
                auto relocated = original;
                const uint32_t delta = loaded_base - kReferenceBase;
                apply_reference_relocations(relocated.data(), offsets, delta);
                uint32_t hash;
                assert(Native::HashReferenceCode(relocated.data(), guard.size, delta, allowlist, hash) && hash == guard.hash);
                if (delta && !offsets.empty()) { // Reproduce why fix 1 rejected this valid code.
                    assert(Native::HashReferenceCode(relocated.data(), guard.size, 0, {}, hash) && hash != guard.hash);
                }
                // A real change must still fail, including all bytes of every
                // relocated address. No wildcard can hide a changed operand.
                for (unsigned i = 0; i < guard.size; ++i) {
                    relocated[i] ^= 1;
                    assert(Native::HashReferenceCode(relocated.data(), guard.size, delta, allowlist, hash) && hash != guard.hash);
                    relocated[i] ^= 1;
                }
                assert(relocated.size() == original.size());
            }
        }
        const uint32_t image_delta = uint32_t(reinterpret_cast<uintptr_t>(image)) - kReferenceBase;
        apply_reference_relocations(code, offsets, image_delta);
    }
    assert(normalized_words == 114);
    assert(TradeRoutes::ValidateNative(image,uint32_t(reinterpret_cast<uintptr_t>(image))-kReferenceBase));
    assert(Fleet::ValidateNative(image,uint32_t(reinterpret_cast<uintptr_t>(image))-kReferenceBase));
    assert(Recovery::ValidateNative(image, uint32_t(reinterpret_cast<uintptr_t>(image))-kReferenceBase));
    assert(Recovery::ValidateTerrain(image, uint32_t(reinterpret_cast<uintptr_t>(image))-kReferenceBase));
    if (exhaustive) {
        auto* predicate=image+0xfa0d40-kReferenceBase;
        DWORD original_protection;
        assert(VirtualProtect(predicate,12,PAGE_EXECUTE_READWRITE,&original_protection));
        alignas(4) unsigned char terrain[0x100]={};
        auto ready=reinterpret_cast<bool(__thiscall*)(void*)>(predicate);
        assert(ready(terrain)); // No load job: the actual native slot returns true.
        const uint32_t job=1; memcpy(terrain+0xfc,&job,4);
        assert(!ready(terrain)); // Active load job: do not sample the terrain.
        predicate[8]^=1;
        assert(!Recovery::ValidateTerrain(image,uint32_t(reinterpret_cast<uintptr_t>(image))-kReferenceBase));
        predicate[8]^=1;
        DWORD ignored; assert(VirtualProtect(predicate,12,original_protection,&ignored));
        puts("Terrain readiness: actual reference predicate is true only without a load job; code tampering rejected.");
        image[0xca35a0-kReferenceBase] ^= 1;
        assert(!Recovery::ValidateNative(image, uint32_t(reinterpret_cast<uintptr_t>(image))-kReferenceBase));
        image[0xca35a0-kReferenceBase] ^= 1;
        unsigned char bytes[8] = {}; uint32_t hash;
        const uint16_t out_of_bounds[] = {5}, overlapping[] = {1, 2}, unsorted[] = {4, 0};
        assert(!Native::HashReferenceCode(nullptr, 0, 0, {}, hash));
        assert(!Native::HashReferenceCode(bytes, 8, 0, {nullptr, 1}, hash));
        assert(!Native::HashReferenceCode(bytes, 8, 0, {out_of_bounds, 1}, hash));
        assert(!Native::HashReferenceCode(bytes, 8, 0, {overlapping, 2}, hash));
        assert(!Native::HashReferenceCode(bytes, 8, 0, {unsorted, 2}, hash));
        puts("ASLR guards: PE-derived operands, lower/preferred/higher bases and every-byte tampering checks passed.");
    }
}

// Optional local evidence check. Captured game bytes are never distributed.
static void verify_captured_reference_guards(const char* path)
{
    FILE* file = fopen(path, "rb"); assert(file);
    uint32_t magic, base, count;
    assert(fread(&magic, 4, 1, file) == 1 && magic == 0x31524342);
    assert(fread(&base, 4, 1, file) == 1 && fread(&count, 4, 1, file) == 1 && count == 4);
    for (unsigned i = 0; i < 4; ++i) {
        const auto& guard = relocated_guards[i];
        uint32_t address, size, hash;
        assert(fread(&address, 4, 1, file) == 1 && address == guard.address);
        assert(fread(&size, 4, 1, file) == 1 && size == guard.size);
        std::vector<unsigned char> code(size);
        assert(fread(code.data(), 1, size, file) == size);
        assert(Native::HashReferenceCode(code.data(), size, base - kReferenceBase,
            Native::CodeRelocations(address), hash) && hash == guard.hash);
    }
    assert(fgetc(file) == EOF); fclose(file);
    puts("Captured live Spore code: all four relocated guards passed the production validator.");
}
