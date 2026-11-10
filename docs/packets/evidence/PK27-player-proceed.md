# Shared player skill proceed and dispatch

Baseline749d4423. CPlayer messages13/15/41/42/90 now own PT_SKILL state and synchronize the mode's transport fields. Rejected capture41 preserves an older skill target/id/range, while following90 still overwrites its level. Resuming the old proceed resolves raw level0 through normal/homun/merc skill storage and then temporary storage, as source mode76 requires. Reaching range invokes42 then13 even when dispatch fails; source15 does not retire PT_SKILL. Stored skill fields survive13. The previous capture/approach/window chain remains covered.

Mode76 retains special10000 capture and adds source20000/20001, ordinary guards, rank fallback, raw wire truncation and154 cart factory/request fallthrough. The checked positive-level shortcut sender remains separate from the raw dispatcher. 0147/39 now stores signed level/SP/range in a separate temporary list before its existing identify recipient. Repeated temporary rows update only four source fields; main skill-list reset does not silently clear them. General mode126 behavior is still open.

The new real-socket replay checks rejected41 followed by90 under state6 and burrow, resume/fallback,15/13 lifetimes, raw32-vs-wire16 boundaries, all five allowed burrow IDs,0147 framing/duplicate metadata, learned/temporary precedence, companion families/state6, shop/chat guards, special packets and cart signed-SP/effect/factory fallthrough. Unrelated player messages cannot overwrite the current world's transport state. Existing capture failure, picker and render checks continue to pass.

## Validation and source review

Deploy-OFF all-target and standalone builds pass; focused2/2 in7.01s, application164/164 in52.31s (-j4) and standalone473/473 in14.00s (-j8). Twenty-two native/Qt frame pairs and real Vulkan capture input pass. DeployON restored configure-only; OldRO unchanged. PK27/PK09.21 and the full plan remain open.

[Primary/selected-PE review](PK27-player-proceed-source-review.json), [interruption audit](PK27-player-proceed-interruption-audit.json), [provenance](PK27-player-proceed-provenance.json), [application checks](PK27-player-proceed-root-ctest.log), [standalone checks](PK27-player-proceed-unit-ctest.log). The original executable was read statically; it was not run.

## Continue with these gates open

Strong agent: review and implement source mode126/general automatic-skill recipients;0147 storage alone is not auto-run acceptance. Review normal shortcut/list arming and pending-skill reset against retained older proceeds, then shared attack/move interruption and ground/touch lifetimes. Do not cancel on state6/burrow transitions or treat mouse-release15 as13. Do not validate raw dispatch like the checked shortcut sender. Retain source four-field temporary duplicate updates and separate storage lifetime.

The port currently represents own-shop ownership with active vending and self-effect state on the local player; source reads the own merchant window and session effect mask. Dedicated UI/session lifetime parity is not closed by these predicates. PK28/29 canonical companion identity producers, companion AI modifiers and full party/guild/event/siege picker/cursor/lock lifetimes remain open. Continue feeding01A3/022F, mode161/01A9, actor action/dialogue01AA, follow/reset/failure, command68/87, broader request discovery, PK47 and independent PK48/49/live/deployed gates. No package or request classification is closed by this checkpoint. The world-entry repair still requires deployment/live retry separately.
