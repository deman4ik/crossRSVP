# Durable RSVP Resume

## Problem Statement

RSVP Mode already writes a durable per-book RSVP Checkpoint, but a reader who exits to Home or the File Browser and later reopens the book cannot reliably continue from the last Active Word. Reopening intentionally starts in Paged Mode, and the first subsequent Mode Switch to RSVP can replace the saved Resume Anchor with the beginning of the displayed page even when the reader has not navigated anywhere. This repeats an arbitrary part of the page instead of the last word the reader actually saw.

The checkpoint can also advance to a word that was prepared but not yet physically presented, and deleting the EPUB cache removes the checkpoint even though native page progress is preserved. Together these gaps make the RSVP position less durable than the ordinary Reading Position it is meant to complement.

## Solution

Preserve the last successfully presented Active Word as the durable per-book RSVP Checkpoint across exit, sleep, restart, and cache rebuild. A reopened book continues to start in Paged Mode, but the saved RSVP Resume Anchor remains eligible until the reader explicitly navigates in Paged Mode. Entering RSVP before such navigation repeats the last presented Active Word on pause. Explicit Paged navigation supersedes the checkpoint and starts RSVP from the newly selected page.

Treat physical presentation acknowledgement as the commit point for durable RSVP progress. Keep the existing bounded, atomic checkpoint format and save cadence, retain the last known-good record on write failure, preserve it when clearing derived EPUB cache data, and remove or invalidate it only when the reading intent or Book Revision makes it unsafe.

## User Stories

1. As an RSVP reader, I want the last Active Word preserved when I leave for Home, so that reopening the book does not reset me to the beginning of a page.
2. As an RSVP reader, I want the last Active Word preserved when I leave for the File Browser, so that choosing the same book resumes my exact place.
3. As an RSVP reader, I want the last Active Word preserved across sleep, so that waking the device does not lose my RSVP position.
4. As an RSVP reader, I want the last Active Word preserved across a device restart, so that durable progress survives normal power cycles.
5. As a reader, I want reopened books to continue starting in Paged Mode, so that RSVP remains explicitly enabled.
6. As a reader reopening a book, I want the saved RSVP position retained while I merely view the restored Paged page, so that opening the book is not mistaken for navigation.
7. As a reader, I want the first Mode Switch back to RSVP to repeat the last Active Word, so that no unseen word is skipped.
8. As a reader, I want restored RSVP to open paused, so that playback never resumes before I am ready.
9. As a reader, I want the checkpoint to identify the exact word rather than only its page, so that layout changes do not reduce resume precision.
10. As a reader, I want only a physically presented word recorded as durable progress, so that a prepared frame that never reached the E-Ink panel is not treated as read.
11. As a reader, I want an in-flight word ignored if rendering or power fails before presentation acknowledgement, so that recovery repeats rather than skips content.
12. As a reader, I want turning a page in Paged Mode to replace the old RSVP position, so that RSVP follows my deliberate navigation.
13. As a reader, I want a chapter or table-of-contents jump to replace the old RSVP position, so that RSVP starts where I deliberately moved.
14. As a reader, I want a bookmark, percentage, or internal-link jump to replace the old RSVP position, so that the old checkpoint does not pull me backward.
15. As a reader, I want RSVP after explicit Paged navigation to begin at the first readable word of the selected page, so that the new position is predictable.
16. As a reader, I want font, margin, orientation, Focus Reading, and other layout changes not to count as navigation, so that repagination does not discard my exact word.
17. As a reader, I want changing pagination settings to preserve the semantic Resume Anchor, so that a page-number change does not move my RSVP position.
18. As an RSVP reader, I want normal exit to save the exact acknowledged Active Word immediately, so that controlled lifecycle transitions lose no progress.
19. As an RSVP reader, I want pause, Mode Switch, chapter transition, and sleep to save progress, so that common interruptions remain safe.
20. As an RSVP reader, I want playback to save periodically, so that sudden power loss can recover near my last position.
21. As an RSVP reader, I accept up to 30 seconds of repetition after sudden power loss, so that the device does not write to SD for every word.
22. As an RSVP reader, I want repeated identical checkpoints to avoid redundant storage writes, so that durability does not add unnecessary SD and mutex traffic.
23. As an RSVP reader, I want a failed checkpoint write to retain the previous valid checkpoint, so that one storage error does not destroy all resume information.
24. As a reader, I want checkpoint write failure not to block leaving the reader, so that navigation remains responsive even when storage is degraded.
25. As a reader, I want recovery to use native page progress when no valid RSVP Checkpoint exists, so that the book still opens safely.
26. As a reader, I want a changed Book Revision to invalidate the old checkpoint, so that an offset never opens unrelated replacement content.
27. As a reader, I want a corrupt or unsupported checkpoint ignored, so that damaged metadata cannot prevent reading.
28. As a reader clearing an EPUB cache, I want the RSVP Checkpoint preserved like native progress, so that rebuilding derived data does not erase my place.
29. As a reader deleting an EPUB, I want its checkpoint removed with it, so that abandoned per-book state does not remain on storage.
30. As a reader who reaches the confirmed end of a book, I want its RSVP Checkpoint cleared, so that the completed book is not offered as unfinished at its last word.
31. As a reader, I want native progress to remain on the final page after completing RSVP, so that the completed location is still represented in Paged Mode.
32. As a reader, I want menu exit, sleep, and restart to follow the same restoration rules, so that resume behavior does not depend on the lifecycle path.
33. As a maintainer, I want the reopen path covered end to end, so that a valid checkpoint cannot silently be bypassed by a future routing change.
34. As a maintainer, I want presentation and durable progress tested as separate events, so that requested frames cannot become acknowledged Reading Position accidentally.
35. As an ESP32-C3 user, I want the solution to reuse bounded checkpoint storage without new per-word allocation, so that exact resume does not compromise the RAM ceiling.

## Implementation Decisions

- Treat this work as a correction to the existing RSVP Checkpoint and Mode Switch contract, not as a new Reading Mode or a second progress subsystem.
- Keep Paged Mode as the default when a book is selected from Home, Recents, the File Browser, after sleep, or after restart.
- Keep the RSVP Checkpoint as a separate, versioned, bounded per-book record reconciled with native progress through a semantic Resume Anchor.
- Distinguish a reopened Paged presentation from explicit Paged navigation. Merely opening or re-rendering the restored page does not supersede the RSVP Checkpoint.
- Mark the saved RSVP Resume Anchor as superseded after an intentional page turn, chapter jump, table-of-contents selection, bookmark jump, percentage jump, or internal-link navigation.
- Do not supersede the checkpoint for font, margin, orientation, Focus Reading, or other layout-only changes. Repagination must preserve semantic position.
- When entering RSVP with an eligible saved checkpoint, restore that checkpoint rather than passing a page-start anchor that bypasses it.
- When entering RSVP after explicit Paged navigation, use the first readable word of the displayed page and treat it as the new Resume Anchor.
- Restore RSVP paused and repeat the last presented Active Word. Never advance automatically to the following word during restoration.
- Separate the requested/prepared frame from the durable presented frame. Only a matching successful presentation acknowledgement may advance the durable anchor and token identity.
- If a new frame is prepared but never acknowledged, lifecycle or periodic persistence continues to use the previous acknowledged frame.
- Preserve existing checkpoint identity fields, Book Revision validation, token verification, atomic replacement, backup recovery, and redundant-write suppression unless a format change is demonstrably required.
- Keep the existing save policy: every 30 seconds of active RSVP reading and on pause, Mode Switch, chapter transition, sleep, and exit.
- Do not write after every word. Sudden power loss may repeat at most the unsaved interval, but must not skip an unpresented word.
- On checkpoint write failure, keep the previous last known-good record, log the error, and allow the lifecycle transition to proceed without a new blocking UI.
- On corrupt, unsupported, stale, unreadable, or wrong-revision checkpoint data, fall back to native Paged progress without exposing unrelated text.
- Preserve the RSVP Checkpoint while deleting and rebuilding derived EPUB cache data, using the same durability expectation as native progress.
- Remove per-book RSVP state when the underlying EPUB is deleted.
- Clear the RSVP Checkpoint only after the session has confirmed end of book; retain native progress on the final page.
- Continue using the storage HAL for all checkpoint and cache operations. No NVS copy, global last-word field, or raw storage access is introduced.
- Add no per-word heap allocation. Any new resume-policy state is fixed-size activity/session state and must stay within the ESP32-C3 memory constraints.

## Testing Decisions

- Use one highest-level deterministic reader lifecycle seam as the primary acceptance test: start RSVP, acknowledge an Active Word, exit to menu, reopen the same book in Paged Mode, enter RSVP without navigating, and assert that the same word appears paused.
- Exercise the real persisted checkpoint and real reader routing in that seam rather than mocking the final restored anchor. The test is valuable only if it catches the current checkpoint-bypass path.
- Extend the same lifecycle seam to cover Home, File Browser, sleep, and restart-equivalent re-entry where the harness supports those transitions.
- Test explicit Paged navigation through externally visible outcomes: after a page turn or jump, RSVP starts from the selected page rather than the older checkpoint.
- Test layout-only changes through externally visible outcomes: repagination keeps the saved Active Word eligible.
- Use the existing RSVP Session host seam to prove that requested frames and acknowledged frames are distinct and that only a matching `FramePresented` event advances durable progress.
- Use existing checkpoint codec and file seams for Book Revision mismatch, corruption, unsupported versions, atomic promotion, backup recovery, identical-write suppression, and failed-write preservation.
- Extend the EPUB cache-deletion behavior test to prove that derived cache data is removed while both native progress and the RSVP Checkpoint survive.
- Test EPUB deletion separately and assert that its per-book RSVP state is removed.
- Test confirmed end-of-book behavior: the RSVP Checkpoint is cleared while native progress remains at the last page.
- Prefer assertions about presented words, paused/playing state, restored Reading Position, and persisted recovery behavior over private flags or container layout.
- Run all RSVP host tests after the final edit and build the default PlatformIO environment once.
- Verify on the project simulator or X3 hardware that exit to menu and re-entry repeat the same displayed word, then verify that a page turn intentionally changes the resume point.
- During hardware verification, monitor free heap and confirm that repeated checkpointing and re-entry show no growth trend or new allocation failure.

## Out of Scope

- Automatically reopening a book directly in RSVP Mode.
- Automatically resuming RSVP playback after reopening.
- Starting from the word after the last presented Active Word.
- Saving a checkpoint after every word.
- Guaranteeing zero rollback after abrupt power loss.
- Replacing native page progress with the RSVP Checkpoint.
- Persisting a global last word outside the per-book cache/state model.
- Recovering a checkpoint whose Book Revision or token identity no longer matches.
- Changing RSVP pacing, controls, ORP selection, Context Line behavior, or Paged ORP Emphasis.
- Adding a new user-facing setting or recovery dialog.
- Redesigning general EPUB cache ownership beyond preserving and deleting RSVP state at the agreed lifecycle boundaries.

## Further Notes

- The existing domain model already defines Reading Position, Resume Anchor, Mode Switch, Active Word, RSVP Checkpoint, and Book Revision; implementation and tests should use those terms consistently.
- The current defect occurs because a valid page-start anchor supplied by a reopened Paged activity takes precedence over the durable RSVP Checkpoint. The fix must make checkpoint eligibility an explicit lifecycle/navigation decision rather than infer it only from anchor validity.
- The durable progress invariant is: the saved Active Word is the most recent matching RSVP frame whose physical presentation was acknowledged. Prepared, prefetched, or contextual words are not durable Reading Position.
- The agreed recovery bias is repetition over omission. When certainty is unavailable, use the last known-good checkpoint or native Paged progress and never skip an unacknowledged word.
