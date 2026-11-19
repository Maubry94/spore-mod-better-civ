#pragma once

static void verify_installer_presets()
{
    // Public Better AI must be exactly equivalent to the five granular choices,
    // without altering prices, specialties, trade or diagnostics.
    Settings granular = mixed, bundled = mixed;
    const char* enabled[] = {
        "<OffensiveTargets>RethinkFailedAttacks", "<StalledOffensives>Stalled180",
        "<VehicleMovement>NearbySpace", "<StrandedTraders>StrandedSell",
        "<SurplusLand>LandRebalance"
    };
    const char* disabled[] = {
        "<OffensiveTargets>TargetsVanilla", "<StalledOffensives>StalledOff",
        "<VehicleMovement>MovementVanilla", "<StrandedTraders>StrandedVanilla",
        "<SurplusLand>LandVanilla"
    };
    for (const auto* line : enabled) apply_option(&granular, line);
    apply_option(&bundled, "<AIBehavior>BetterAI");
    assert(memcmp(&bundled, &granular, sizeof bundled) == 0);
    for (const auto* line : disabled) apply_option(&granular, line);
    apply_option(&bundled, "<AIBehavior>AIVanilla");
    assert(memcmp(&bundled, &granular, sizeof bundled) == 0);

    for (int on = 0; on != 2; ++on) {
        granular = bundled = mixed;
        apply_option(&granular, on ? "<RivalTrade>FirstRivalRoute" : "<RivalTrade>RivalTradeVanilla");
        apply_option(&granular, on ? "<TradeUnderFire>StayOnCourse" : "<TradeUnderFire>RetreatVanilla");
        apply_option(&bundled, on ? "<TradeBehavior>BetterTrade" : "<TradeBehavior>TradeVanilla");
        assert(memcmp(&bundled, &granular, sizeof bundled) == 0);
    }

    // A previous install's settings must not silently invert a public choice,
    // regardless of whether those stale lines occur before or after the preset.
    for (int on = 0; on != 2; ++on) for (int stale_first = 0; stale_first != 2; ++stale_first) {
        bundled = mixed;
        OptionReader reader(&bundled);
        for (int phase = 0; phase != 2; ++phase) {
            if (phase == stale_first) {
                reader.consume(on ? "<AIBehavior>BetterAI" : "<AIBehavior>AIVanilla");
                reader.consume(on ? "<TradeBehavior>BetterTrade" : "<TradeBehavior>TradeVanilla");
            } else {
                for (int i = 0; i != 5; ++i) reader.consume(on ? disabled[i] : enabled[i]);
                reader.consume(on ? "<Commerce>CommerceVanilla" : "<Commerce>PremierPartenaire");
                reader.consume(on ? "<RepliCommercial>RepliVanilla" : "<RepliCommercial>MaintenirTrajet");
            }
        }
        reader.consume("<AIDiagnostics>DiagnosticsOn");
        reader.finish();
        granular = mixed;
        apply_option(&granular, on ? "<AIBehavior>BetterAI" : "<AIBehavior>AIVanilla");
        apply_option(&granular, on ? "<TradeBehavior>BetterTrade" : "<TradeBehavior>TradeVanilla");
        granular.ai_diagnostics = 1;
        assert(memcmp(&bundled, &granular, sizeof bundled) == 0);
    }
    // No bundled choice: developers can still mix rules independently.
    bundled = mixed;
    OptionReader reader(&bundled);
    reader.consume("<StalledOffensives>Stalled240");
    reader.consume("<VehicleMovement>RetryBlockedPaths");
    reader.consume("<AIBehavior>BetterAITrailing");
    reader.finish();
    assert(bundled.stalled_seconds == 240 && bundled.retry_blocked_paths == 1);
    assert(!bundled.rethink_offensives && !bundled.retire_stranded && !bundled.rebalance_land);
    puts("Installer presets: equivalence, Vanilla reset, stale-setting precedence and developer independence passed.");
}
