# Companion status windows

The selected `Ref/UIWindowMgr.cpp` R key branch at line 4190 toggles the homunculus status with Alt and the mercenary status with Ctrl. It requires the basic info window and the corresponding alive flag. MakeWindow selectors 113 and 132 require a known job. The port uses the selected -1 initial job sentinel.

The 280 by 180 windows share a display model built from the corresponding session snapshot. The native and Qt views show both combat stat sets, HP and SP, plus homunculus experience, hunger, intimacy, and accessory or mercenary expiry, faith, calls, and kills. Layout and field anchors are `UIHomunInfoWnd::OnCreate` and `OnDraw` at `Ref/UIFrameWnd.cpp:8310` and `:8772`, and `UIMerInfoWnd::OnCreate` and `OnDraw` at `:9917` and `:10166`. This checkpoint uses basic framing and English captions. Source skin art, the accessory icon, persisted position, localization, and companion action controls remain open.

The replay checks unknown-job gating, separate windows, snapshot display, ordinary actor removal, homunculus death, skill 244 source scoping, and explicit close. All 164 application checks and 473 standalone checks passed. The skill tree window source and assets and the installed OldRO executable matched baseline. Live gameplay verification remains open.
