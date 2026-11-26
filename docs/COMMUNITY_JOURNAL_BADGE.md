# War Journal notification glyph collision

Source-only draft recipe. Default invocation only prepares output under --work-dir outside the source checkout. --apply explicitly writes to a closed local game with backup. Requires Python3.10+,Pillow>=10,the existing texconv, current repository font metadata and the player's own original game/texture dump. No game-derived output belongs in the repository.

```powershell
python tools/community_prompts/journal_badge.py --game-root YOUR_GAME --pack YOUR_PACK/4D53082D --work-dir OUTSIDE_REPO/work
```

The port already implements controller/keyboard activity detection and variant switching; this tool does not replace that system. See PR_DESCRIPTION.md in the submission package for exact limitations and validation.
