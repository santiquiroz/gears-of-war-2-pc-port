# English checkpoint message

UpdatingStrings.SavingContent_Message currently says "Saving content. Please don't turn off your console." This source-only tool changes that exact known English entry to "Saving content. Please don't turn off your PC." It does not change save behavior, other strings, other locales or texture packs.

Run python tools/community_prompts/checkpoint_message.py --game-root YOUR_GAME --work-dir OUTSIDE_REPO/work. Default is output-only. --apply requires a closed game, creates a backup, atomically replaces the same-size local source bundle and invalidates only its generated cache stamp. Unknown/ambiguous layouts or messages are rejected. Generated bundles must not be committed or distributed.

Seven synthetic tests cover ASCII/UTF16, idempotence, exact targeting and malformed/unknown inputs. A future upstream implementation should perform equivalent locale-aware cache editing rather than require manual asset changes. This recipe is English-only and independent of controllers and world/UI texture packs.
