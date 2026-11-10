{
    const std::filesystem::path iniPath = GetOpenMidgardIniPath();
    const bool hadIni = std::filesystem::exists(iniPath);
    std::string originalIni;
    if (hadIni) {
        std::ifstream input(iniPath, std::ios::binary);
        originalIni.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        Require(input.good() || input.eof(), "could not preserve settings INI before replay");
    }
    struct RestoreIni {
        std::filesystem::path path;
        bool existed;
        std::string contents;
        ~RestoreIni()
        {
            std::error_code error;
            if (existed) {
                std::filesystem::create_directories(path.parent_path(), error);
                std::ofstream output(path, std::ios::binary | std::ios::trunc);
                output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
            } else {
                std::filesystem::remove(path, error);
            }
        }
    } restore{iniPath, hadIni, originalIni};

    const auto rewriteWithoutSection = [&](const std::string& removedSection) {
        std::istringstream lines(originalIni);
        std::ostringstream output;
        std::string line;
        bool skip = false;
        while (std::getline(lines, line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
                skip = line.substr(1, line.size() - 2) == removedSection;
            }
            if (!skip) output << line << '\n';
        }
        std::filesystem::create_directories(iniPath.parent_path());
        std::ofstream file(iniPath, std::ios::binary | std::ios::trunc);
        file << output.str();
        Require(file.good(), "could not rewrite isolated settings fixture");
    };

    rewriteWithoutSection("SkillUseLevelInfo");
    std::vector<std::pair<std::string, int>> missing{{"untouched", 41}};
    Require(!LoadSettingsIniIntValues("SkillUseLevelReplayMissing", missing), "batch load accepted missing section");
    Require(missing.size() == 1 && missing[0].first == "untouched" && missing[0].second == 41,
        "missing section changed batch defaults");
    CSession missingSession;
    missingSession.SetSkillUseLevel(30, 77);
    Require(!missingSession.LoadSkillUseLevels(7) && missingSession.GetSkillUseLevel(30) == 77,
        "missing skill-level section changed session values");

    const std::string batchSection = "SkillUseLevelReplayBatch";
    std::vector<std::pair<std::string, int>> values{{"MissingKey", 17}, {"BadKey", 23}};
    Require(SaveSettingsIniIntValues(batchSection.c_str(), {{"BadKey", 0}}), "could not create batch fixture section");
    Require(SaveSettingsIniString(batchSection.c_str(), "BadKey", "not-an-int"), "could not create invalid batch fixture value");
    Require(LoadSettingsIniIntValues(batchSection.c_str(), values), "batch load rejected existing section");
    Require(values[0].second == 17 && values[1].second == 23, "missing or invalid batch keys changed supplied defaults");
    Require(SaveSettingsIniString(batchSection.c_str(), "Unrelated", "keep-me"), "could not create unrelated batch fixture key");
    Require(SaveSettingsIniIntValues(batchSection.c_str(), {{"Duplicate", 3}, {"Duplicate", 9}}), "batch save failed");
    std::vector<std::pair<std::string, int>> saved{{"Duplicate", 0}};
    Require(LoadSettingsIniIntValues(batchSection.c_str(), saved) && saved[0].second == 9, "duplicate batch save was not last-wins");
    std::string unrelated;
    Require(TryLoadSettingsIniString(batchSection.c_str(), "Unrelated", &unrelated) && unrelated == "keep-me",
        "batch save removed an unrelated key");

    CSession session;
    Require(session.GetSkillUseLevel(1) == 0 && session.GetSkillUseLevel(1038) == 0 &&
        session.GetHomunSkillUseLevel(8000) == 0 && session.GetHomunSkillUseLevel(8016) == 0 &&
        session.GetMerSkillUseLevel(8200) == 0 && session.GetMerSkillUseLevel(8237) == 0,
        "fresh session skill-level defaults or array boundaries are incorrect");
    session.SetSkillUseLevel(0, 99);
    session.SetSkillUseLevel(1039, 99);
    session.SetHomunSkillUseLevel(7999, 99);
    session.SetHomunSkillUseLevel(8017, 99);
    session.SetMerSkillUseLevel(8199, 99);
    session.SetMerSkillUseLevel(8238, 99);
    Require(session.GetSkillUseLevel(1) == 0 && session.GetSkillUseLevel(1038) == 0 &&
        session.GetHomunSkillUseLevel(8000) == 0 && session.GetHomunSkillUseLevel(8016) == 0 &&
        session.GetMerSkillUseLevel(8200) == 0 && session.GetMerSkillUseLevel(8237) == 0,
        "invalid or reserved skill IDs mutated valid storage");
    Require(!session.SaveSkillUseLevels(-1) && !session.SaveSkillUseLevels(256), "skill-level save accepted out-of-range slots");
    Require(!session.LoadSkillUseLevels(-1) && !session.LoadSkillUseLevels(256), "skill-level load accepted out-of-range slots");

    const char* homunName = SKILL_INFO::GetSkillIdNameBySkillId(8000);
    const char* mercName = SKILL_INFO::GetSkillIdNameBySkillId(8200);
    Require(homunName && mercName && std::string(homunName) == mercName,
        "expected homunculus/mercenary primary-name alias is absent");
    session.SetSkillUseLevel(30, -1);
    session.SetSkillUseLevel(4, 12);
    session.SetHomunSkillUseLevel(8000, 5);
    session.SetMerSkillUseLevel(8200, 6);
    Require(session.SaveSkillUseLevels(7), "raw skill-level save failed");
    int aliased = 0;
    Require(TryLoadSettingsIniInt("SkillUseLevelInfo", (std::string(homunName) + "7").c_str(), &aliased) && aliased == 6,
        "primary-name alias did not persist the later mercenary value");

    CSession charEight;
    Require(charEight.SaveSkillUseLevels(8), "could not save separate character defaults");
    CSession loadedSeven;
    Require(loadedSeven.LoadSkillUseLevels(7), "saved character skill levels did not load");
    Require(loadedSeven.GetSkillUseLevel(30) == -1, "raw signed level was normalized during persistence");
    Require(loadedSeven.GetSkillUseLevel(4) == 0, "passive skill level was not cleared on load");
    Require(loadedSeven.GetHomunSkillUseLevel(8000) == 0 && loadedSeven.GetMerSkillUseLevel(8200) == 0,
        "non-level-using alias skill was not cleared on load");
    CSession loadedEight;
    Require(loadedEight.LoadSkillUseLevels(8) && loadedEight.GetSkillUseLevel(30) == 0,
        "per-character values leaked between slots");

    session.SetSkillUseLevel(30, 65536);
    Require(session.SaveSkillUseLevels(8), "wide raw skill-level save failed");
    CSession wideEight;
    Require(wideEight.LoadSkillUseLevels(8) && wideEight.GetSkillUseLevel(30) == 65536,
        "raw 65536 skill level was normalized during persistence");
    CSession stillSeven;
    Require(stillSeven.LoadSkillUseLevels(7) && stillSeven.GetSkillUseLevel(30) == -1,
        "saving another character overwrote the first character");
}
