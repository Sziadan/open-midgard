    {
        struct RestoreTips {ServiceType service=g_serviceType;int startup=g_showTipsAtStartup;
            CTipOfTheDay tips=g_tipOfTheDay;
            ~RestoreTips(){g_serviceType=service;g_showTipsAtStartup=startup;g_tipOfTheDay=std::move(tips);}
        } restoreTips;
        const auto write=[](const std::filesystem::path& path,const std::string& bytes){
            if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
            std::ofstream file(path,std::ios::binary);file.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));Require(file.good(),"tip fixture write failed");};
        const auto localized=[](u16 language){auto folder=SourceTipLanguageFolder(language);std::replace(folder.begin(),folder.end(),'\\','/');return std::filesystem::path(folder)/"tipofday.txt";};
        Require(SourceTipLanguageFolder(0x0407)=="german\\" && SourceTipLanguageFolder(0x0810)=="italian\\"
            && SourceTipLanguageFolder(0x0416)=="portuguese\\" && SourceTipLanguageFolder(0x041f)=="turkish\\"
            && SourceTipLanguageFolder(0x041d)=="english\\","tip folders used service/keyboard rather than OS primary language");
        g_serviceType=ServiceKorea;CTipOfTheDay missing;
        Require(!missing.m_bTryLoading && missing.GetNumTip()==0 && missing.m_bTryLoading && missing.GetTip(0).empty(),"missing tip load/lazy/empty guard differs");
        write("tipOfTheday.txt","first\r\nsecond#tail\r\nline#ignored final fragment");
        Require(missing.GetNumTip()==0,"lazy tip getter retried a failed load");
        Require(missing.Load(0x0407) && missing.GetNumTip()==2,"explicit tip retry did not load fallback file");
#if RO_PLATFORM_WINDOWS
        const CTipOfTheDay::Tip first{"first","second"},second{"tail","line"};
#else
        const CTipOfTheDay::Tip first{"first\r","second"},second{"tail\r","line"};
#endif
        Require(missing.GetTip(0)==first && missing.GetTip(1)==second && missing.GetTip(-1)==first && missing.GetTip(INT_MAX)==first,
            "fallback CRT lines/delimiters/index fallback differ");
        Require(missing.Load(0x0407) && missing.GetNumTip()==4 && missing.GetTip(2)==first,"explicit source Load must append instead of clearing prior tips");
        write("tipOfTheday_e.txt","ENGLISH FALLBACK#");write("tipOfTheday_j.txt","JAPANESE FALLBACK#");
        g_serviceType=ServiceAmerica;CTipOfTheDay america;
        Require(america.Load(0x0407) && america.GetTip(0)==CTipOfTheDay::Tip{"ENGLISH FALLBACK"},"America fallback filename differs");
        g_serviceType=ServiceJapan;CTipOfTheDay japan;
        Require(japan.Load(0x0407) && japan.GetTip(0)==CTipOfTheDay::Tip{"JAPANESE FALLBACK"},"Japan fallback filename differs");
        g_serviceType=ServiceGermany;CTipOfTheDay other;
        Require(other.Load(0x0407) && other.GetTip(0)==first,"other service must use base fallback filename");
        write(localized(0x0407),"zero\r\none#\n#tail");g_serviceType=ServiceAmerica;CTipOfTheDay german;
        Require(german.Load(0x0407) && german.GetNumTip()==2 && german.GetTip(0)==CTipOfTheDay::Tip{"zero\r","one"}
            && german.GetTip(1)==CTipOfTheDay::Tip{"",""},"localized CFile path must preserve raw CR and blank source lines");
        write("tipofday.txt","BASE RESOURCE#");CTipOfTheDay base;
        Require(base.Load(0x0410) && base.GetTip(0)==CTipOfTheDay::Tip{"BASE RESOURCE"},"LanguageFile original-name fallback did not precede service fallback");
        write(localized(0x041d),"OS ENGLISH#");g_serviceType=ServiceGermany;CTipOfTheDay osLanguage;
        Require(osLanguage.Load(0x041d) && osLanguage.GetTip(0)==CTipOfTheDay::Tip{"OS ENGLISH"},"localized tip folder followed service rather than supplied OS language");
        write(localized(0x0410),"");CTipOfTheDay empty;
        Require(empty.Load(0x0410) && empty.GetNumTip()==0 && empty.GetTip(42).empty(),"existing empty localized file must suppress other fallbacks and remain safe");
        write(localized(0x0416),std::string{'A','\0','B','\n','C','#','D'});CTipOfTheDay nul;
        Require(nul.Load(0x0416) && nul.GetTip(0)==CTipOfTheDay::Tip{"A","C"},"tip parser changed source bounded C-string/NUL semantics");
        write(localized(0x041f),std::string("raw ")+char(0xe9)+"#LF only\nno complete tip");CTipOfTheDay raw;
        Require(raw.Load(0x041f) && raw.GetNumTip()==1 && raw.GetTip(0)==CTipOfTheDay::Tip{std::string("raw ")+char(0xe9)},"tip loader decoded bytes or committed unfinished data");
        g_serviceType=ServiceKorea;CTipOfTheDay korea;
        Require(korea.Load(0x0407) && korea.GetTip(0)==first,"Korea must bypass localized and original-name CFile branches");
        write("tipOfTheday.txt","");CTipOfTheDay emptyCrt;
        Require(emptyCrt.Load(0x0407) && emptyCrt.GetNumTip()==0,"empty fallback file did not succeed with zero tips");
#if RO_PLATFORM_WINDOWS
        write("tipOfTheday.txt",std::string("before#")+char(0x1a)+"after#");CTipOfTheDay eof;
        Require(eof.Load(0x0407) && eof.GetNumTip()==1 && eof.GetTip(0)==CTipOfTheDay::Tip{"before"},"fallback CRT Ctrl-Z tail was parsed as initialized resource data");
#endif
        // Selected ReadRegistry deliberately overrides even a saved ON value.
        Require(SaveSourceTipOptions(7) && g_showTipsAtStartup==7 && LoadSettingsIniInt("Option","SHOWTIPSATSTARTUP",-1)==7,
            "tip checkbox adapter lost raw immediate value or persistence");
        LoadSourceTipOptions();Require(g_showTipsAtStartup==0,"successful settings-store read did not preserve selected forced-OFF policy");
        const auto settings=GetOpenMidgardIniPath();const auto parked=settings.parent_path()/"tip-options-original.ini";
        std::filesystem::rename(settings,parked);g_showTipsAtStartup=9;LoadSourceTipOptions();
        Require(g_showTipsAtStartup==9,"failed settings-store open reset existing/default source flag");std::filesystem::rename(parked,settings);
        std::cout<<"{\"case_id\":\"source_tip_data\",\"contract_id\":\"PK09.21\",\"actual_resource_and_CRT_io\":true,\"lazy_append_delimiters\":true,\"selected_startup_policy\":true,\"outcome\":\"PASS\"}\n";
    }
