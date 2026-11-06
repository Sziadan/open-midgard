#include "../../../src/core/UiMessageStrings.h"

void ChatSourceLabelsReplay()
{
    SetCase("chat_source_language_labels", "PK09.21", "source-st-bt-message-table-labels",
        "UiMessageStrings IDs1291/1292 -> main, detached history, mini and Qt projection labels");

    ro::UiMessageStrings positional;
    std::string fixture;
    for (int index = 0; index <= 1292; ++index) {
        if (index == 1291) {
            fixture += "#";
        } else if (index == 1292) {
            fixture += "translated-BT#";
        } else {
            fixture += "row-" + std::to_string(index) + "#";
        }
    }
    positional.Parse(fixture);
    Require(positional.Get(1291, "fallback-ST").empty(),
        "known empty ST translation was replaced by fallback");
    Require(positional.Get(1292, "fallback-BT") == "translated-BT",
        "BT label did not retain its selected message-table index");
    Require(positional.Get(1293, "fallback-missing") == "fallback-missing",
        "missing message-table entry did not retain fallback");

    struct ResourceRestore {
        std::filesystem::path cwd = std::filesystem::current_path();
        std::filesystem::path directory;
        int folderFirst = g_readFolderFirst;
        ~ResourceRestore() {
            std::error_code ec;
            std::filesystem::current_path(cwd, ec);
            g_readFolderFirst = folderFirst;
            if (!directory.empty()) std::filesystem::remove_all(directory, ec);
        }
    } restore;
    restore.directory = restore.cwd / ("chat-source-label-resource-"
        + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(restore.directory);
    std::string messageTable;
    for (int index = 0; index <= 1292; ++index) {
        if (index == 1291) messageTable += "#";
        else if (index == 1292) messageTable += "Translated-BT#";
        else messageTable += "fixture-" + std::to_string(index) + "#";
    }
    {
        std::ofstream output(restore.directory / "msgstringtable.txt", std::ios::binary);
        output.write(messageTable.data(), static_cast<std::streamsize>(messageTable.size()));
        Require(static_cast<bool>(output), "write isolated source label table failed");
    }
    std::filesystem::current_path(restore.directory);
    g_readFolderFirst = 1;

    UINewChatWnd main;
    ChatTabDisplay st{}, bt{};
    Require(main.GetTabDisplay(0, &st) && main.GetTabDisplay(1, &bt),
        "main source tabs were unavailable");
    Require(std::string(st.label).empty() && std::string(bt.label) == "Translated-BT",
        "main source tabs did not preserve known-empty and translated IDs1291/1292");
    Require(main.GetSourceTabLabel(0).empty() && main.GetSourceTabLabel(1) == "Translated-BT",
        "main source label projection diverged from native labels");

    UISubChatHisWnd historySt(0), historyBt(1);
    UISubChatMiniWnd mini;
    Require(historySt.GetSourceLabel().empty() && historyBt.GetSourceLabel() == "Translated-BT",
        "detached history labels did not preserve IDs1291/1292");
    Require(mini.GetSourceLabel(0).empty() && mini.GetSourceLabel(1) == "Translated-BT",
        "mini labels did not preserve IDs1291/1292");

    {
        std::ofstream output(restore.directory / "msgstringtable.txt", std::ios::binary | std::ios::trunc);
        output << "short-table-row-0#short-table-row-1#";
        Require(static_cast<bool>(output), "write isolated short source label table failed");
    }
    UINewChatWnd missingMain;
    UISubChatHisWnd missingHistorySt(0), missingHistoryBt(1);
    UISubChatMiniWnd missingMini;
    ChatTabDisplay missingSt{}, missingBt{};
    Require(missingMain.GetTabDisplay(0, &missingSt) && missingMain.GetTabDisplay(1, &missingBt)
        && std::string(missingSt.label) == "ST" && std::string(missingBt.label) == "BT",
        "main source tabs did not fall back for missing message-table indices");
    Require(missingHistorySt.GetSourceLabel() == "ST" && missingHistoryBt.GetSourceLabel() == "BT"
        && missingMini.GetSourceLabel(0) == "ST" && missingMini.GetSourceLabel(1) == "BT",
        "detached or mini labels did not fall back for missing message-table indices");

    std::cout << "{\"case_id\":\"chat_source_language_labels\",\"contract_id\":\"PK09.21\","
        << "\"synthetic_resource\":true,\"known_empty_distinct_from_missing\":true,"
        << "\"main_detached_mini\":true,\"missing_index_owner_fallbacks\":true,\"outcome\":\"PASS\"}\n";
}
