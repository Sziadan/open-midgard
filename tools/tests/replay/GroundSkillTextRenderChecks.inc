    {
        const auto oldFocus=g_windowMgr.m_editWindow;
        for(const int id:{84,101}){
            auto* window=static_cast<UIGroundSkillTextWnd*>(g_windowMgr.MakeWindow(id));
            Require(window && window->m_w==280 && window->m_h==120,"source text window geometry missing");
            window->Move(20,20);window->GetEditCtrl()->SetText("Ground skill fixture");
            Require(window->GetEditCtrl()->m_x==42 && window->GetEditCtrl()->m_y==70
                && window->GetEditCtrl()->m_maxchar==78,"source text edit position/limit differs");
            Require(g_windowMgr.IsWhisperPreferenceInputWindow(window) && g_windowMgr.IsWhisperPreferenceInputWindow(window->GetEditCtrl()),
                "ground prompt/child missing native Qt input ownership");
            Require(window->GetColorChip().IsValid() && window->GetColorChipRgb(2,2)==0x123456
                && window->GetColorChipRgb(20,6)==0xa1b2c3 && window->GetColorChipRgb(-1,0)==0xff0000
                && window->GetColorChipRgb(32,0)==0xff0000 && window->GetColorChipRgb(0,8)==0xff0000,
                "selected color-chip coordinates/channel extraction/red bounds differ");
            const auto pointer=[&](UINT message,int x,int y){
                WindowProc(g_hMainWnd,message,message==WM_LBUTTONUP?0:MK_LBUTTON,
                    MAKELPARAM(UiScaleLogicalToRawCoordinate(x),UiScaleLogicalToRawCoordinate(y)));
            };
            pointer(WM_LBUTTONDOWN,30,55);
            Require(window->IsFrameDragging() && g_windowMgr.m_captureWindow==window && window->m_transTarget==128,
                "native frame down failed capture/128 transition target");
            pointer(WM_MOUSEMOVE,70,80);
            Require(window->m_x==60 && window->m_y==45 && window->GetEditCtrl()->m_x==82 && window->GetEditCtrl()->m_y==95,
                "frame move accumulated drift or detached its edit child");
            pointer(WM_MOUSEMOVE,-10,-5);
            Require(window->m_x==-20 && window->m_y==-40,"source unregistered frame drag acquired a client clamp");
            pointer(WM_LBUTTONUP,400,300);
            Require(!window->IsFrameDragging() && !g_windowMgr.m_captureWindow && window->m_transTarget==255,
                "outside frame release failed ownership/255 transition target");
            window->OnMouseMove(100,100);Require(window->m_x==-20 && window->m_y==-40,"uncaptured frame followed pointer");
            window->Move(20,20);
#if RO_PLATFORM_WINDOWS
            if (id == 84) {
                ArgbDibSurface opacityDib;
                Require(opacityDib.EnsureSize(view.width(), view.height()), "opacity DIB failed");
                constexpr unsigned background = 0xff182c50u;
                const auto draw = [&](u32 tick) {
                    std::fill_n(opacityDib.GetPixels(), view.width()*view.height(), background);
                    window->DrawFrameAtTick(opacityDib.GetDC(), tick);
                    GdiFlush();
                };
                const auto pixel = [&](int x, int y) { return opacityDib.GetPixels()[y*view.width()+x] & 0xffffffu; };
                const auto* edit = window->GetEditCtrl();
                const std::array<POINT,3> samples{{{25,79},{edit->m_x+edit->m_w-4,edit->m_y+8},{212,119}}};
                draw(window->m_transTime);
                std::array<unsigned,3> opaque{};
                for (std::size_t i = 0; i < samples.size(); ++i) {
                    opaque[i] = pixel(samples[i].x, samples[i].y);
                    Require(opaque[i] != (background & 0xffffffu), "opacity fixture missed the prompt layer");
                }
                pointer(WM_LBUTTONDOWN,30,58);
                Require(window->IsFrameDragging(), "opacity fixture did not capture the prompt");
                const auto captureTick = window->m_transTime;
                g_windowMgr.ClearDirtyVisualState();
                Require(window->IsUpdateNeed() && g_windowMgr.HasDirtyVisualStateExcludingRoMap(),
                    "overlay cache would stop drawing the transition after clearing ordinary dirty state");
                draw(captureTick + 16);
                Require(window->m_trans == 254, "frame alpha did not advance by one after 16 ms");
                draw(captureTick + 32);
                Require(window->m_trans == 252, "frame alpha did not apply elapsed time at each source draw");
                draw(captureTick + 6400);
                Require(window->m_trans == 128, "frame alpha did not clamp to the drag target");
                const auto blended = [](unsigned source) {
                    unsigned result = 0;
                    for (const unsigned shift : {0u, 8u, 16u}) {
                        const auto top = ((source >> shift) & 255) * 128;
                        const auto bottom = ((background >> shift) & 255) * 127;
                        result |= ((top + 127) / 255 + bottom / 255) << shift;
                    }
                    return result;
                };
                for (std::size_t i = 0; i < samples.size(); ++i)
                    Require(pixel(samples[i].x, samples[i].y) == blended(opaque[i]),
                        "prompt frame, edit, or button was not blended as part of one layer");
                ArgbDibSurface upload;
                RECT uploadBounds{};
                const RECT client{0,0,view.width(),view.height()};
                Require(g_windowMgr.BuildGroundSkillPromptOverlay(upload, client, captureTick + 6400, uploadBounds),
                    "modern prompt texture could not be composed");
                Require(uploadBounds.left == window->m_x && uploadBounds.top == window->m_y
                    && upload.GetWidth() == window->m_w && upload.GetHeight() == window->m_h,
                    "modern prompt texture was not cropped to the frame");
                for (std::size_t i = 0; i < samples.size(); ++i) {
                    unsigned expected = 0x80000000u;
                    for (const unsigned shift : {0u, 8u, 16u})
                        expected |= ((((opaque[i] >> shift) & 255) * 128 + 127) / 255) << shift;
                    const auto index = (samples[i].y - uploadBounds.top) * upload.GetWidth()
                        + samples[i].x - uploadBounds.left;
                    Require(upload.GetPixels()[index] == expected,
                        "modern upload lost the prompt's premultiplied frame, edit, or button alpha");
                }
                Require(pixel(2,2) == (background & 0xffffffu), "opacity paint touched the scene outside the prompt");
                window->Move(-20,20);
                draw(captureTick + 6400);
                Require(pixel(samples[1].x-40,samples[1].y) == blended(opaque[1]),
                    "partially clipped drag lost the translucent edit layer");
                window->Move(20,20);
                pointer(WM_LBUTTONUP,400,300);
                const auto releaseTick = window->m_transTime;
                draw(releaseTick + 16);
                Require(window->m_trans == 129, "release did not begin restoring alpha");
                draw(releaseTick + 6400);
                Require(window->m_trans == 255 && pixel(samples[0].x, samples[0].y) == opaque[0],
                    "release did not restore the opaque frame");
                g_windowMgr.ClearDirtyVisualState();
                Require(!window->IsUpdateNeed(), "settled opacity left the prompt animated");
            }
#endif
            pointer(WM_LBUTTONDOWN,212,119);pointer(WM_MOUSEMOVE,350,180);pointer(WM_LBUTTONUP,350,180);
            g_windowMgr.OnProcess();Require(g_windowMgr.ContainsWindow(window) && window->GetButtonState(118)!=2,
                "outside button release submitted or left a pressed latch");
            pointer(WM_LBUTTONDOWN,50,75);pointer(WM_LBUTTONUP,50,75);
            WindowProc(g_hMainWnd,WM_CHAR,'!',0);
            Require(std::string(window->GetEditCtrl()->GetText())=="Ground skill fixture!","native edit focus/typing did not reach model");
            window->GetEditCtrl()->SetText("x");
            WindowProc(g_hMainWnd, WM_CHAR, 0xd83d, 0);
            Require(std::string(window->GetEditCtrl()->GetText()) == "x", "partial surrogate entered the prompt");
            WindowProc(g_hMainWnd, WM_CHAR, 0xde42, 0);
            Require(std::string(window->GetEditCtrl()->GetText()) == "x\xf0\x9f\x99\x82", "native surrogate pair was not committed once");
            WindowProc(g_hMainWnd, WM_KEYDOWN, VK_BACK, 0);
            WindowProc(g_hMainWnd, WM_CHAR, 8, 0);
            Require(std::string(window->GetEditCtrl()->GetText()) == "x", "Backspace did not remove exactly one multibyte character");
            window->GetEditCtrl()->SetText(std::string(77, 'a').c_str());
            WindowProc(g_hMainWnd, WM_CHAR, 0x00e9, 0);
            Require(std::string(window->GetEditCtrl()->GetText()) == std::string(77, 'a') + " ", "source limit did not replace the cut character with padding");
            WindowProc(g_hMainWnd, WM_KEYDOWN, VK_RETURN, 0);
            WindowProc(g_hMainWnd, WM_CHAR, 13, 0);
            WindowProc(g_hMainWnd, WM_KEYDOWN, VK_ESCAPE, 0);
            WindowProc(g_hMainWnd, WM_CHAR, 27, 0);
            g_windowMgr.OnProcess();
            Require(g_windowMgr.ContainsWindow(window), "physical key injected a prompt button message");
            if (id == 84) {
                struct QtInputWindow {
                    RoNativeWindowHandle handle{};
                    ~QtInputWindow() { if (handle) RoQtDestroyMainWindow(handle); }
                } inputWindow;
                Require(RoQtCreateMainWindow("Ground prompt input replay", 120, 80, false, nullptr, &inputWindow.handle), "Qt input host creation failed");
                auto* host = RoQtGetQWindow(inputWindow.handle);
                window->GetEditCtrl()->SetText("");
                QKeyEvent key(QEvent::KeyPress, Qt::Key_E, Qt::NoModifier, QString::fromUtf8("\xc3\xa9"));
                QCoreApplication::sendEvent(host, &key);
                QInputMethodEvent commit;
                commit.setCommitString(QString::fromUtf8("\xe3\x81\x82"));
                QCoreApplication::sendEvent(host, &commit);
                Require(std::string(window->GetEditCtrl()->GetText()) == "\xc3\xa9\xe3\x81\x82", "Qt key or IME commit did not reach the prompt exactly once");
                QKeyEvent left(QEvent::KeyPress,Qt::Key_Left,Qt::NoModifier);
                QCoreApplication::sendEvent(host,&left);
                QKeyEvent shiftLeft(QEvent::KeyPress,Qt::Key_Left,Qt::ShiftModifier);
                QCoreApplication::sendEvent(host,&shiftLeft);
                Require(window->GetEditCtrl()->m_selectionOrigin == 2 && window->GetEditCtrl()->m_selectionCursor == 0,
                    "Qt shift-left did not select one complete UTF-8 character");
                QInputMethodEvent replacement;
                replacement.setCommitString(QStringLiteral("Q"));
                QCoreApplication::sendEvent(host,&replacement);
                Require(std::string(window->GetEditCtrl()->GetText()) == "Q\xe3\x81\x82", "IME commit did not replace the selection");

                g_codePage = 932;
                window->HandleEditKey(VK_END, false);
                QCoreApplication::sendEvent(host, &shiftLeft);
                const auto copied = window->CopySelectionBytes();
                Require(copied && *copied == "\x82\xa0", "reverse selection did not copy exact CP932 bytes");
                QKeyEvent shiftedCopy(QEvent::KeyPress, Qt::Key_X, Qt::ShiftModifier | Qt::ControlModifier);
                QCoreApplication::sendEvent(host, &shiftedCopy);
                Require(window->CopySelectionBytes() == copied, "Ctrl+Shift+X bypassed source Shift precedence");
                constexpr char paste[] = "\x82\xa2\r\n\0ignored";
                window->PasteSelectionBytes(std::string_view(paste, sizeof(paste) - 1));
                Require(std::string(window->GetEditCtrl()->GetText()) == "Q\xe3\x81\x84\r\n", "clipboard insertion lost CP932, CRLF, or the NUL boundary");
                Require(!window->CopySelectionBytes(), "clipboard insertion did not collapse the selection");
                window->GetEditCtrl()->SetText(std::string(77, 'a').c_str());
                window->PasteSelectionBytes("\x82\xa0");
                Require(std::string(window->GetEditCtrl()->GetText()) == std::string(77, 'a') + " ", "clipboard insertion bypassed the wire byte limit");

                g_codePage = 65001;
                window->GetEditCtrl()->SetText("a\xf0\x9f\x99\x82" "b");
                window->HandleEditKey(VK_HOME, false);
                window->HandleEditKey(VK_RIGHT, false);
                const auto queryInput = [&](Qt::InputMethodQuery field) {
                    QInputMethodQueryEvent query(field);
                    QCoreApplication::sendEvent(host, &query);
                    return query.value(field);
                };
                const auto initialCaret = queryInput(Qt::ImCursorRectangle).toRect();
                QInputMethodEvent preedit(QStringLiteral("PQ"), {{QInputMethodEvent::Cursor, 1, 0, {}}});
                QCoreApplication::sendEvent(host, &preedit);
                Require(queryInput(Qt::ImSurroundingText).toString() == QString::fromUtf8("a\xf0\x9f\x99\x82" "b"),
                    "preedit leaked into committed surrounding text");
                Require(queryInput(Qt::ImCursorRectangle).toRect().x() > initialCaret.x(),
                    "preedit cursor did not advance within the edit");
                QInputMethodEvent cancel;
                QCoreApplication::sendEvent(host, &cancel);
                Require(queryInput(Qt::ImCursorRectangle).toRect() == initialCaret,
                    "canceled preedit left the cursor displaced");
                window->HandleEditKey(VK_RIGHT, false);
                Require(queryInput(Qt::ImCursorPosition).toInt() == 3, "IME query returned bytes instead of UTF-16 units");
                QInputMethodEvent range;
                range.setCommitString(QStringLiteral("Z"), -2, 3);
                QCoreApplication::sendEvent(host, &range);
                Require(std::string(window->GetEditCtrl()->GetText()) == "aZ", "IME range replacement split or skipped a character");

                window->GetEditCtrl()->SetText("keep");
                window->HandleEditKey(VK_HOME, true);
                QCoreApplication::sendEvent(host, &preedit);
                QCoreApplication::sendEvent(host, &cancel);
                Require(queryInput(Qt::ImCurrentSelection).toString() == QStringLiteral("keep"),
                    "canceling composition did not restore the selected committed text");
            }
            g_codePage = 949;
            window->GetEditCtrl()->SetText("");
            window->CommitText(std::u16string(40, u'\uac00'));
            Require(std::strlen(window->GetEditCtrl()->GetText()) == 39*3, "prompt limit used UTF-8 bytes instead of CP949 bytes");
            window->OnKeyDown(VK_BACK);
            Require(std::strlen(window->GetEditCtrl()->GetText()) == 38*3, "Backspace truncated text expanded by the codepage conversion");
            g_codePage = 65001;
            window->GetEditCtrl()->SetText("a\xc3\xa9z");
            WindowProc(g_hMainWnd,WM_KEYDOWN,VK_HOME,0);
            WindowProc(g_hMainWnd,WM_KEYDOWN,VK_RIGHT,0);
            WindowProc(g_hMainWnd,WM_KEYDOWN,VK_DELETE,0);
            Require(std::string(window->GetEditCtrl()->GetText()) == "az", "native Delete split a multibyte character");
            window->GetEditCtrl()->SetText("Focus selection");
            g_windowMgr.m_editWindow = nullptr;
            pointer(WM_LBUTTONDOWN,50,75); pointer(WM_LBUTTONUP,50,75);
            Require(window->GetEditCtrl()->m_selectionOrigin == 0 && window->GetEditCtrl()->m_selectionCursor == 15,
                "focus entry did not select the existing text");
            ArgbDibSurface selectionDib;
            Require(selectionDib.EnsureSize(view.width(),view.height()),"selection DIB failed");
            g_windowMgr.DrawVisibleWindowsToHdc(selectionDib.GetDC(),true);
            const QImage selection(reinterpret_cast<uchar*>(selectionDib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32);
            Require(selection.pixelColor(46,70)==QColor("#de9944"),"selection highlight did not use the source palette sample");
            if (id == 84) {
                g_codePage = 932;
                const auto* edit = window->GetEditCtrl();
                const int below = edit->m_y + edit->m_h + 6;
                const int above = edit->m_y - 34 - 6;
                const int sampleX = edit->m_x + 8;
                const auto renderCandidates = [&](RECT client, int y) {
                    std::fill_n(selectionDib.GetPixels(), view.width()*view.height(), 0xff181b20u);
                    window->DrawImeCandidates(selectionDib.GetDC(), client);
                    return selection.pixelColor(sampleX, y);
                };
                window->SetImeCandidates({{"\xe3\x81\x82"}, 0, 0, 0});
                Require(renderCandidates({0,0,view.width(),view.height()}, below + 10) == QColor("#de9944"),
                    "Japanese candidate did not draw its source highlight below the edit");
#if RO_PLATFORM_WINDOWS
                ArgbDibSurface candidateUpload;
                RECT candidateBounds{};
                Require(g_windowMgr.BuildGroundSkillPromptOverlay(candidateUpload,
                    {0,0,view.width(),view.height()}, timeGetTime(), candidateBounds), "modern candidate layer failed");
                Require(candidateUpload.GetPixels()[(below + 10)*candidateUpload.GetWidth()+sampleX] == 0xffde9944u
                    && candidateUpload.GetPixels()[0] == 0u, "modern candidate layer lost opaque popup and transparent scene pixels");
#endif
                Require(renderCandidates({0,0,view.width(),below + 20}, above + 10) == QColor("#de9944"),
                    "candidate did not flip above the edit when the bottom edge overflowed");
#if defined(_WIN32)
                WindowProc(g_hMainWnd, WM_IME_NOTIFY, IMN_CLOSECANDIDATE, 1);
#else
                window->SetImeCandidates({});
#endif
                Require(renderCandidates({0,0,view.width(),view.height()}, below + 10) == QColor("#181b20"),
                    "native close notification left the candidate popup visible");
                g_codePage = 950;
                window->HandleEditKey(VK_HOME, false);
                window->RecordImeKeystroke('1');
                window->RecordImeKeystroke('2');
                window->RecordImeKeystroke('3');
                auto cells = window->ImeKeystrokeCells();
                Require(cells && (*cells)[0] == "\xe3\x84\x89" && (*cells)[3] == "\xcb\x87",
                    "Traditional Chinese keys did not occupy their selected-client slots");
                window->RecordImeKeystroke(VK_BACK);
                cells = window->ImeKeystrokeCells();
                Require(cells && (*cells)[3].empty(), "IME Backspace did not remove the last stroke");
                window->RecordImeKeystroke(0xbf);
#if defined(_WIN32)
                WindowProc(g_hMainWnd, WM_IME_COMPOSITION, 0, GCS_COMPSTR);
#else
                window->RetainLastImeKeystroke();
#endif
                cells = window->ImeKeystrokeCells();
                Require(cells && (*cells)[0] == "??" && (*cells)[2] == "11",
                    "composition update did not keep just the latest selected-client stroke");
                window->DrawImeKeystrokes(selectionDib.GetDC(), {0,0,view.width(),view.height()});
                Require(selection.pixelColor(edit->m_x + 1, below + 1) == QColor("#123456"),
                    "keystroke window did not draw the source interior color");
#if defined(_WIN32)
                WindowProc(g_hMainWnd, WM_IME_ENDCOMPOSITION, 0, 0);
#else
                window->ClearImeKeystrokes();
#endif
                Require(!window->ImeKeystrokeCells(), "IME end left the keystroke window visible");
                g_codePage = 65001;

                window->GetEditCtrl()->SetText("WW");
                window->HandleEditKey(VK_HOME, false);
                window->HandleEditKey(VK_END, true);
                window->SetNativeComposition(u"WWWW");
                g_windowMgr.DrawVisibleWindowsToHdc(selectionDib.GetDC(), true);
                QFont font("Segoe UI");
                font.setPixelSize(12);
                const QFontMetrics metrics(font);
                const int four = metrics.horizontalAdvance(QStringLiteral("WWWW"));
                const int six = metrics.horizontalAdvance(QStringLiteral("WWWWWW"));
                const int nativeSample = edit->m_x + 3 + (four + six) / 2;
                Require(selection.pixelColor(nativeSample, edit->m_y) == QColor("#de9944"),
                    "native composition replaced selected display text instead of extending the source highlight");
                Require(std::string(edit->GetText()) == "WW", "native composition changed the committed prompt");
                window->CancelComposition();
                window->HandleEditKey(VK_HOME, false);
                window->SetNativeComposition(u"WWWW");
                bool sampledPhase = false;
                for (int attempt = 0; attempt < 8 && !sampledPhase; ++attempt) {
                    const auto before = timeGetTime() / 500;
                    g_windowMgr.DrawVisibleWindowsToHdc(selectionDib.GetDC(), true);
                    if (before != timeGetTime() / 500) continue;
                    const QColor expected = (before & 1) == 0 ? QColor("#de9944") : QColor(edit->m_r, edit->m_g, edit->m_b);
                    Require(selection.pixelColor(edit->m_x + 4, edit->m_y) == expected,
                        "collapsed native composition did not follow the source blink phase");
                    sampledPhase = true;
                }
                Require(sampledPhase, "unable to capture a stable composition blink phase");
                window->CancelComposition();
                window->HandleEditKey(VK_END, true);
            }
            WindowProc(g_hMainWnd,WM_CHAR,'!',0);
            Require(std::string(window->GetEditCtrl()->GetText()) == "!", "native character did not replace focus selection");
            window->GetEditCtrl()->SetText("Ground skill fixture!");
            for(int i=0;i<6;++i)Require(window->GetFrameBitmap(i).IsValid(),"source text frame fixture failed raw-key asset lookup");
            ArgbDibSurface dib;Require(dib.EnsureSize(view.width(),view.height()),"ground text DIB failed");
            std::fill_n(dib.GetPixels(),view.width()*view.height(),0xff181b20u);
            g_windowMgr.DrawVisibleWindowsToHdc(dib.GetDC(),true);
            const auto image=QImage(reinterpret_cast<uchar*>(dib.GetPixels()),view.width(),view.height(),view.width()*4,QImage::Format_RGB32).copy(20,20,280,120);
            Require(image.pixelColor(3,3)==QColor("#d24240") && image.pixelColor(3,117)==QColor("#b09060")
                && image.pixelColor(192,99)==QColor("#c0a080"),"source text frame/footer/button placement differs");
            Require(image.pixelColor(50,80)==QColor("#123456") && image.pixelColor(0,70)==QColor("#a1b2c3"),"prompt fill/border bypassed color-chip samples");
            int ink=0;for(int y=51;y<65;++y)for(int x=25;x<200;++x)if(image.pixelColor(x,y).red()<120)++ink;
            Require(ink>30,"ground text edit disappeared in composed Qt/native UI");
            Require(window->GetCaption()=="Talkbox Window" && window->GetPrompt()=="Enter text for ground skill","source text message string IDs differ");
            Require(image.save(output+QString("/ground-text-%1-native.png").arg(id)),"ground text image save failed");
            pointer(WM_LBUTTONDOWN,30,55);g_windowMgr.DeleteWindow(window);
            Require(!g_windowMgr.m_captureWindow && GetCapture()!=g_hMainWnd,"owner deletion retained native capture");
        }
        {
            CGameMode gameMode;
            struct RestoreSkinContext {
                QByteArray iniDirectory = qgetenv("OPEN_MIDGARD_DATA_DIR");
                bool hadIniDirectory = qEnvironmentVariableIsSet("OPEN_MIDGARD_DATA_DIR");
                CGameMode* mode = g_modeMgr.GetCurrentGameMode();
                int codePage = g_codePage;
                ~RestoreSkinContext()
                {
                    if (hadIniDirectory) qputenv("OPEN_MIDGARD_DATA_DIR", iniDirectory);
                    else qunsetenv("OPEN_MIDGARD_DATA_DIR");
                    SkillPacketReplayAccess::SetCurrent(mode);
                    g_codePage = codePage;
                }
            } restoreSkin;
            Require(!g_modeMgr.GetCurrentMode() || restoreSkin.mode, "skin fixture requires a gameplay or empty mode");
            const auto iniDirectory = std::filesystem::absolute("ground-skin-settings");
            std::filesystem::create_directories(iniDirectory);
            Require(qputenv("OPEN_MIDGARD_DATA_DIR", QByteArray::fromStdString(iniDirectory.string())),
                "could not isolate the skin settings fixture");
            Require(GetOpenMidgardIniPath().parent_path() == iniDirectory, "skin fixture would write outside its settings directory");
            const auto bitmap = [](const std::filesystem::path& path, int width, int height, QColor color) {
                std::filesystem::create_directories(path.parent_path());
                QImage image(width,height,QImage::Format_RGB32);
                image.fill(color);
                Require(image.save(QString::fromStdString(path.string()), "BMP"), "ground skin bitmap save failed");
            };
            bitmap("skin/PacketSkinA/basic_interface/titlebar_left.bmp",12,17,QColor("#123abc"));
            bitmap("skin/PacketSkinA/colorchip.bmp",32,8,QColor("#26b478"));
            bitmap("skin/PacketSkinA/btn_ok.bmp",28,18,QColor("#8e6040"));
            bitmap("skin/PacketSkinB/colorchip.bmp",32,8,QColor("#dc9246"));
            for (const char* name : {"sysbox_lu.bmp","sysbox_mu.bmp","sysbox_ru.bmp","sysbox_lm.bmp",
                    "sysbox_rm.bmp","sysbox_ld.bmp","sysbox_md.bmp","sysbox_rd.bmp"}) {
                bitmap(std::filesystem::path("skin/PacketSkinA") / name,14,14,QColor("#317ac5"));
                bitmap(std::filesystem::path("skin/PacketSkinB") / name,14,14,QColor("#a25d19"));
            }
            Require(SaveSettingsIniString("Graphics","SkinName","PacketSkinA"), "skin setting save failed");
            SkillPacketReplayAccess::SetCurrent(nullptr);
            auto* prompt = static_cast<UIGroundSkillTextWnd*>(g_windowMgr.MakeWindow(84));
            Require(prompt && prompt->GetColorChipRgb(2,2) == 0x123456u,
                "selected skin applied outside the gameplay mode");
            prompt->Move(20,20);
            SkillPacketReplayAccess::SetCurrent(&gameMode);
            Require(g_windowMgr.MakeWindow(84) == prompt, "skin reload replaced the managed prompt");
            const auto rgb = [](const shopui::BitmapPixels& pixels) {
                return pixels.IsValid() ? pixels.pixels.front() & 0xffffffu : 0u;
            };
            Require(prompt->GetColorChipRgb(2,2) == 0x26b478u
                && rgb(prompt->GetFrameBitmap(0)) == 0x123abcu
                && rgb(prompt->GetFrameBitmap(1)) == 0x4042d2u
                && rgb(prompt->GetButton(118)->bitmaps[0]) == 0x8e6040u,
                "selected skin precedence or per-resource fallback differed");
            g_codePage = 932;
            prompt->SetImeCandidates({{"candidate"},0,0,0});
            const auto candidateCorner = [&] {
                ArgbDibSurface surface;
                Require(surface.EnsureSize(view.width(),view.height()), "skin popup DIB failed");
                prompt->DrawImeCandidates(surface.GetDC(), {0,0,view.width(),view.height()}, true);
                const auto* edit = prompt->GetEditCtrl();
                return surface.GetPixels()[(edit->m_y+edit->m_h+7)*surface.GetWidth()+edit->m_x+1] & 0xffffffu;
            };
            Require(candidateCorner() == 0x317ac5u, "selected skin candidate lattice did not load");
            Require(SaveSettingsIniString("Graphics","SkinName","PacketSkinB"), "replacement skin setting save failed");
            Require(g_windowMgr.MakeWindow(84) == prompt && prompt->GetColorChipRgb(2,2) == 0xdc9246u
                && rgb(prompt->GetFrameBitmap(0)) == 0xd24240u && candidateCorner() == 0xa25d19u,
                "cached prompt did not refresh skin artwork, palette, and candidate lattice together");

            QByteArray zip;
            QByteArray directory;
            const auto u16le = [](QByteArray& bytes, unsigned value) {
                bytes.append(static_cast<char>(value));
                bytes.append(static_cast<char>(value >> 8));
            };
            const auto u32le = [&](QByteArray& bytes, unsigned value) {
                u16le(bytes, value);
                u16le(bytes, value >> 16);
            };
            const auto zipBitmap = [&](const QByteArray& name, int width, int height, QColor color,
                    bool deflate, bool corruptCrc = false) {
                QImage image(width, height, QImage::Format_RGB32);
                image.fill(color);
                QByteArray bmp;
                QBuffer buffer(&bmp);
                Require(buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "BMP"), "ZIP skin fixture encode failed");
                unsigned crc = 0xffffffffu;
                for (const unsigned char byte : bmp) {
                    crc ^= byte;
                    for (int bit = 0; bit < 8; ++bit)
                        crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
                }
                crc = ~crc;
                if (corruptCrc) crc ^= 1;
                QByteArray encoded = bmp;
                if (deflate) {
                    // qCompress adds a four-byte length and a zlib wrapper.
                    const auto wrapped = qCompress(bmp);
                    Require(wrapped.size() > 10, "ZIP skin fixture compression failed");
                    encoded = wrapped.mid(6, wrapped.size() - 10);
                }
                const auto offset = static_cast<unsigned>(zip.size());
                QByteArray header;
                u16le(header, 20);
                u16le(header, 0);
                u16le(header, deflate ? 8 : 0);
                u32le(header, 0);
                u32le(header, crc);
                u32le(header, static_cast<unsigned>(encoded.size()));
                u32le(header, static_cast<unsigned>(bmp.size()));
                u16le(header, static_cast<unsigned>(name.size()));
                u16le(header, 0);
                u32le(zip, 0x04034b50u);
                zip += header + name + encoded;
                u32le(directory, 0x02014b50u);
                u16le(directory, 20);
                directory += header;
                u32le(directory, 0);
                u16le(directory, 0);
                u32le(directory, 0);
                u32le(directory, offset);
                directory += name;
            };
            zipBitmap("COLORCHIP.BMP",32,8,QColor("#42ac6c"),true);
            zipBitmap("PacketSkinZip/Basic_Interface/Titlebar_Left.BMP",12,17,QColor("#314972"),true);
            zipBitmap("skin\\PacketSkinZip\\btn_ok.bmp",28,18,QColor("#723149"),false);
            zipBitmap("basic_interface/titlebar_right.bmp",12,17,QColor("#993164"),false,true);
            const auto centralOffset = static_cast<unsigned>(zip.size());
            zip += directory;
            u32le(zip, 0x06054b50u);
            u32le(zip, 0);
            u16le(zip, 4);
            u16le(zip, 4);
            u32le(zip, static_cast<unsigned>(directory.size()));
            u32le(zip, centralOffset);
            u16le(zip, 0);
            {
                QFile archive(QStringLiteral("skin/PacketSkinZip.zip"));
                Require(archive.open(QIODevice::WriteOnly) && archive.write(zip) == zip.size(), "ZIP skin fixture save failed");
            }
            Require(SaveSettingsIniString("Graphics","SkinName","PacketSkinZip"), "ZIP skin setting save failed");
            Require(g_windowMgr.MakeWindow(84) == prompt && prompt->GetColorChipRgb(2,2) == 0x42ac6cu
                && rgb(prompt->GetFrameBitmap(0)) == 0x314972u && rgb(prompt->GetFrameBitmap(1)) == 0x4042d2u
                && rgb(prompt->GetButton(118)->bitmaps[0]) == 0x723149u,
                "ZIP skin entry lookup, decoding, or CRC fallback differed");
            Require(!std::filesystem::exists("_tmpskin_/PacketSkinZip"), "ZIP skin created an extraction directory");
            Require(SaveSettingsIniString("Graphics","SkinName","<Basic Skin>"), "basic skin setting save failed");
            Require(g_windowMgr.MakeWindow(84) == prompt && prompt->GetColorChipRgb(2,2) == 0x123456u,
                "basic skin did not restore the original palette");
            UIOptionWnd options;
            options.SetShow(1);
            UIOptionWnd::DisplayData settings;
            Require(options.GetDisplayDataForQt(&settings), "could not read settings geometry");
            const auto graphics = std::find_if(settings.tabs.begin(), settings.tabs.end(),
                [](const auto& tab) { return tab.label == "Graphics"; });
            Require(graphics != settings.tabs.end(), "graphics settings tab missing");
            options.OnLBtnDown(graphics->x + 2, graphics->y + 2);
            Require(options.GetDisplayDataForQt(&settings), "could not read graphics controls");
            const auto skinRow = std::find_if(settings.graphicsRows.begin(), settings.graphicsRows.end(),
                [](const auto& row) { return row.label == "Ground prompt skin"; });
            Require(skinRow != settings.graphicsRows.end() && skinRow->value == "Basic Skin",
                "graphics skin control did not show the default choice");
            options.OnLBtnDown(skinRow->prevX + 2, skinRow->prevY + 2);
            Require(LoadSettingsIniString("Graphics","SkinName","") == "PacketSkinZip"
                && prompt->GetColorChipRgb(2,2) == 0x42ac6cu,
                "settings control did not select the ZIP or refresh the open prompt");
            options.OnLBtnDown(skinRow->nextX + 2, skinRow->nextY + 2);
            options.OnLBtnDown(skinRow->nextX + 2, skinRow->nextY + 2);
            Require(LoadSettingsIniString("Graphics","SkinName","") == "PacketSkinA"
                && prompt->GetColorChipRgb(2,2) == 0x26b478u,
                "settings control did not wrap through Basic Skin to the first directory");
            g_windowMgr.DeleteWindow(prompt);
        }
        g_windowMgr.m_editWindow=oldFocus;
        QFile proof(output+"/ground-text-window-render-results.json");Require(proof.open(QIODevice::WriteOnly),"ground text proof open failed");
        proof.write(QJsonDocument(QJsonObject{{"outcome","PASS"},{"native_frames",2},{"source_ids_84_101",true},
            {"source_edit_geometry_and_limit",true},{"source_key_frame_footer_button_samples",true},
            {"valid_colorchip_sample_and_paint",true},{"native_windowproc_input",true},{"unregistered_frame_drag",true},
            {"pointer_capture_and_outside_release",true},{"custom_skin_resource_acceptance",true},
            {"zip_skin_resource_acceptance",true},
            {"ground_prompt_skin_picker_replay",true},
            {"opacity_compositor_acceptance",static_cast<bool>(RO_PLATFORM_WINDOWS)},
            {"modern_prompt_upload_alpha",static_cast<bool>(RO_PLATFORM_WINDOWS)},
            {"native_character_and_qt_commit",true},{"source_codepage_editor_limit",true},{"source_selection_and_replacement",true},
            {"source_clipboard_bytes_and_selection",true},{"qt_preedit_cursor_and_replacement",true},
            {"source_candidate_layout_and_native_close",true},{"source_tc_keystroke_cells_and_lifecycle",true},
            {"source_native_composition_highlight",true},
            {"native_keyboard_acceptance",false},{"source_colorchip_acceptance",false},
            {"skill_tree_window_changed",false}}).toJson(QJsonDocument::Compact));
    }
