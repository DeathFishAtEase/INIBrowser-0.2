#include "IBRender.h"
#include "IBFront.h"
#include "Global.h"
#include "FromEngine/RFBump.h"
#include "FromEngine/global_timer.h"
#include "IBB_RegType.h"
#include "IBB_ModuleAlt.h"
#include <imgui_internal.h>
#include <shlwapi.h>
#include <format>

extern wchar_t CurrentDirW[];
extern bool ShouldCloseShellLoop;
extern bool GotoCloseShellLoop;
extern const char* LinkGroup_IniName;

std::string ExtName(const std::string& ss);//拓展名，无'.' 
std::string FileNameNoExt(const std::string& ss);//文件名，无'.' 
std::string FileName(const std::string& ss);//文件名
std::wstring FileName(const std::wstring& ss);//文件名
bool IsExistingDir(const wchar_t* Path);

std::wstring RemoveSpec(std::wstring W)
{
    PathRemoveFileSpecW(W.data());
    W.resize(wcslen(W.data()));
    return W;
}

namespace IBR_ProjectManager
{
    InfoStack<StdMessage> ActionAfterClose;
    std::atomic_bool OutputComplete{ false };

#define _IN_SAVE_THREAD
#define _IN_FRONT_THREAD
#define _IN_RENDER_THREAD
#define _IN_ANY_THREAD

    //ClearCurrentPopup的调用起始点没有线程要求
    inline void _IN_RENDER_THREAD SetWaitingPopup()
    {
        IBR_PopupManager::SetCurrentPopup(
            std::move(IBR_PopupManager::Popup{}
                .CreateModal(u8"请稍候", false)
                .SetFlag(
                    ImGuiWindowFlags_NoTitleBar |
                    ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoMove |
                    ImGuiWindowFlags_NoResize)
                .SetSize({ FontHeight * 10.0F,FontHeight * 6.0F })
                .PushTextBack(u8"请稍候……")));
    }

    bool _IN_RENDER_THREAD Create()
    {
        auto& proj = IBF_Inst_Project.Project;
        proj.ProjName = L"未命名";
        proj.Path = L"";
        proj.LastOutputDir = L"";
        proj.IsNewlyCreated = true;
        proj.ChangeAfterSave = false;
        proj.LastUpdate = GetSysTimeMicros();
        proj.CreateTime = GetSysTimeMicros();
        proj.CreateVersionMajor = VersionMajor;
        proj.CreateVersionMinor = VersionMinor;
        proj.CreateVersionRelease = VersionRelease;
        IBF_Inst_Project.CurrentProjectRID = GlobalRnd();
        IBB_DefaultRegType::ClearModuleCount();
        IsProjectOpen = true;
        return true;
    }
    bool _IN_RENDER_THREAD Close() {
        IsProjectOpen = false;
        IBB_DefaultRegType::ClearModuleCount();
        {
            IBD_RInterruptF(x);
            IBF_Inst_Project.CurrentProjectRID = 0;
            auto& proj = IBF_Inst_Project.Project;
            proj.Clear();

            IBR_Inst_Project.MaxID = 0;
            IBR_Inst_Project.IBR_SectionMap.clear();
            IBR_Inst_Project.IBR_Rev_SectionMap.clear();
            IBG_Undo.Clear();
            IBR_WorkSpace::Close();
            IBR_SelectMode::CancelSelectMode();
            IBR_EditFrame::Clear();
            IBR_PopupManager::ClearRightClickMenu();
        }
        if (GotoCloseShellLoop)
        {
            ShouldCloseShellLoop = true;
        }
        if (!ActionAfterClose.Empty())
        {
            IBRF_CoreBump.SendToR({ []() {
                for (auto& Action : ActionAfterClose.Release())
                    Action();
            } });
        }
        return true;
    }
    bool _IN_ANY_THREAD IsOpen() { return IsProjectOpen; }

    void _IN_RENDER_THREAD AskFilePath(const InsertLoad::SelectFileType& Type, BOOL(_stdcall* Proc)(LPOPENFILENAMEW), const std::function<void(const std::optional<std::wstring>&)>& _IN_SAVE_THREAD Next)
    {
        IBS_Push([=]()
            {
                auto Ret = InsertLoad::SelectFileName(MainWindowHandle, Type, Proc, false);
                if (Ret.Success)Next(Ret.RetBuf);
                else Next(std::nullopt);
            });
    }
    void _IN_RENDER_THREAD AskLoadPath(const std::function<void(const std::optional<std::wstring>&)>& _IN_SAVE_THREAD Next)
    {
        AskFilePath(InsertLoad::SelectFileType{ CurrentDirW ,L"打开项目",
            L"",L"项目文件(" ExtensionNameW ")\0*" ExtensionNameW "\0所有文件(*.*)\"0*.*\0\0"}, ::GetOpenFileNameW, Next);
    }
    void _IN_RENDER_THREAD AskSavePath(const std::function<void(const std::optional<std::wstring>&)>& _IN_SAVE_THREAD Next)
    {
        AskFilePath(InsertLoad::SelectFileType{ CurrentDirW ,L"保存项目",
            IBF_Inst_Project.Project.IsNewlyCreated ? std::wstring(L"未命名" ExtensionNameW ) : IBF_Inst_Project.Project.ProjName
            ,L"项目文件(" ExtensionNameW ")\0*" ExtensionNameW "\0所有文件 (*.*)\0*.*\0\0" }, ::GetSaveFileNameW, Next);
    }

    void _IN_SAVE_THREAD Save(std::wstring Path, const std::function<void(bool)>& _IN_SAVE_THREAD Next)
    {
        if (!Path.ends_with(ExtensionNameW))Path += ExtensionNameW;
        IBR_RecentManager::Push(Path);
        IBR_RecentManager::Save();

        auto SigF = std::make_shared<std::atomic_bool>(false);
        auto SigR = std::make_shared<std::atomic_bool>(false);
        auto Sent = std::make_shared<std::atomic_bool>(false);
        IBRF_CoreBump.SendToR({ [=]()
            {
                IBR_Inst_Project.Save(IBS_Inst_Project);
                *SigR = true;
                if (*SigF && (!*Sent)) { *Sent = true; IBS_Push([=]() {Next(IBS_Inst_Project.Save(Path)); }); }
            },nullptr });
        IBRF_CoreBump.SendToF({ [=]()
            {
                IBF_Inst_Project.Save(IBS_Inst_Project);
                *SigF = true;
                if (*SigR && (!*Sent)) { *Sent = true; IBS_Push([=]() {Next(IBS_Inst_Project.Save(Path)); }); }
            } });
    }
    void _IN_SAVE_THREAD Load(const std::wstring& Path, const std::function<void(bool)>& _IN_SAVE_THREAD Next)
    {
        if (IBS_Inst_Project.Load(Path))
        {
            std::shared_ptr<bool> SigF = std::make_shared<bool>(false), SigR = std::make_shared<bool>(false);
            IBRF_CoreBump.SendToR({ [=]()
                {
                    IBR_Inst_Project.Load(IBS_Inst_Project);
                    *SigR = true;
                },nullptr });
            IBRF_CoreBump.SendToF({ [=]()
                {
                    IBF_Inst_Project.Load(IBS_Inst_Project);
                    *SigF = true;
                    while (!*SigR);
                    Next(true);
                } });
        }
        else Next(false);
    }
    void _IN_RENDER_THREAD AskIfSave(const std::function<void(bool)>& _IN_RENDER_THREAD Next)
    {
        IBR_PopupManager::SetCurrentPopup(std::move(
            IBR_PopupManager::Popup{}
        .CreateModal(u8"请稍候", false, []() {IBR_HintManager::SetHint(u8"已取消", HintStayTimeMillis); })
            .SetFlag(
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoResize)
            .SetSize({ FontHeight * 10.0F,FontHeight * 6.0F })
            .PushTextBack(u8"你是否要保存当前项目？")
            .PushMsgBack([=]()
                {
                    if (ImGui::Button(u8"保存##AskIfSave"))
                    {
                        IBRF_CoreBump.SendToR({ [=]() {  Next(true); IBR_PopupManager::ClearCurrentPopup(); }, nullptr });
                    }ImGui::SameLine(); ImGui::SetCursorPosX(ImGui::GetCursorPosX() + FontHeight * 0.7f);
                    if (ImGui::Button(u8"不保存##AskIfSave"))
                    {
                        IBRF_CoreBump.SendToR({ [=]() {  Next(false); IBR_PopupManager::ClearCurrentPopup(); }, nullptr});
                    }ImGui::SameLine(); ImGui::SetCursorPosX(ImGui::GetCursorPosX() + FontHeight * 0.7f);
                    if (ImGui::Button(u8"取消##AskIfSave"))
                    {
                        IBRF_CoreBump.SendToR({ [=]() {IBR_PopupManager::ClearCurrentPopup();
                        GotoCloseShellLoop = false; glfwSetWindowShouldClose(PreLink::window, false); }, nullptr });
                    }
                })));
    }

    void _IN_FRONT_THREAD OutputRegister(std::vector<std::vector<std::string>>& TextPieces)
    {
        auto& Proj = IBF_Inst_Project.Project;
        std::unordered_map<std::string, std::vector<std::string>> Reg;
        for (auto& Ini : Proj.Inis)
            for (auto& [SN, Sec] : Ini.Secs)
                Reg[Sec.Register].push_back(Sec.Name);
        for (auto& [N, R] : Reg)
        {
            auto& RegType = IBB_DefaultRegType::GetRegType(N);
            if (!RegType.Export)continue;
            IBB_Project_Index Idx(RegType.IniType);
            Idx.GetIni(Proj);
            //MessageBoxA(NULL, Idx.GetIni(Proj)->Name.c_str(), std::to_string(Idx.Ini.Index).c_str(), MB_OK);
            std::string V;
            V += '[';V += N;V += "]\n";
            for (auto& v : R){ V += v; V += '='; V += v; V += '\n'; }
            TextPieces[Idx.Ini.Index].push_back(std::move(V));
        }
    }
    void _IN_FRONT_THREAD Output(const std::wstring& Path, const std::vector<std::wstring>& TargetIniPath,const std::set<IBB_Section_Desc>& IgnoredSection, bool TriggerAfterAction)
    {
        using namespace std::string_literals;
        std::string TimeNowU8();
        auto& Inis = IBF_Inst_Project.Project.Inis;
        if (TargetIniPath.size() != Inis.size())
        {
            MessageBoxA(NULL, "错误：INI导出路径与实际INI数目不匹配！", "导出文档", MB_OK);
            IBRF_CoreBump.SendToR({ [] {
                IBR_HintManager::SetHint(u8"导出失败。",HintStayTimeMillis);
                IBR_PopupManager::ClearCurrentPopup(); } });
            return;
        }

        std::map<IBB_Section_Desc, std::string> DisplayRev;
        for (auto& [K, V] : IBF_Inst_Project.DisplayNames)DisplayRev[V] = K;

        size_t N = TargetIniPath.size();
        std::vector<std::vector<std::string>>TextPieces;
        TextPieces.resize(N);

        OutputRegister(TextPieces);
        for (size_t I = 0; I < N; I++)
        {
            //MessageBoxW(NULL, TargetIniPath[I].c_str(), L"dsfsd", MB_OK);
            for (auto& [SN, Sec] : Inis[I].Secs)
            {
                IBB_Section_Desc Desc = { Inis[I].Name, Sec.Name };
                std::string V;
                V += ';'; V += DisplayRev[Desc];  V += '\n';
                if (IgnoredSection.contains(Desc) || Sec.IsLinkGroup || Sec.IsComment())continue;
                V += '['; V += Sec.Name;
                if(!Sec.Inherit.empty())
                { V+="]:["; V+=Sec.Inherit; }
                V += "]\n";
                V += Sec.GetText(false, true);
                TextPieces[I].push_back(std::move(V));
            }
        }

        for (size_t I = 0; I < N; I++)
        {
            if (Inis[I].Name == LinkGroup_IniName)continue;//不导出这个

            ExtFileClass F;
            if (F.Open(TargetIniPath[I].c_str(), L"w"))
            {
                F.PutStr(";"s + AppName + " " + Version); F.Ln();
                F.PutStr(u8";从 "+ UnicodetoUTF8(IBF_Inst_Project.Project.ProjName) + u8" 导出"); F.Ln();
                F.PutStr(u8";生成于 " + TimeNowU8()); F.Ln();
                for (auto& V : TextPieces[I])
                {
                    F.PutStr(V);
                    F.Ln();
                    F.Ln();
                }
            }
            else MessageBoxW(NULL, (L"无法打开" + TargetIniPath[I] + L"以写入").c_str(), L"无法导出", MB_OK);
        }

        if (TriggerAfterAction)
        {
            IBRF_CoreBump.SendToR({ [] {
                IBR_HintManager::SetHint(u8"导出完成。",HintStayTimeMillis);
                IBR_PopupManager::ClearCurrentPopup(); } });
            if (IBF_Inst_Setting.OpenFolderOnOutput())
                ShellExecuteW(NULL, L"open", L"explorer.exe", Path.c_str(), NULL, SW_SHOWNORMAL);
        }

        OutputComplete = true;
        IBR_PopupManager::ClearPopupDelayed();
    }
    std::set<IBB_Section_Desc> _IN_RENDER_THREAD GetIgnoredSection()
    {
        std::set<IBB_Section_Desc> Result;
        for (auto& [id, sd] : IBR_Inst_Project.IBR_SectionMap)
            if (sd.Ignore)Result.insert(sd.Desc);
        return Result;
    }

    void _IN_RENDER_THREAD OpenAction()
    {
        SetWaitingPopup();
        AskLoadPath([](auto ws)
            {
                if (ws)
                {
                    IBR_RecentManager::Push(ws.value());
                    IBR_RecentManager::Save();
                    Load(ws.value(), [](bool OK) {IBRF_CoreBump.SendToR({ [=]()
                            {
                                IBR_PopupManager::ClearCurrentPopup();
                                if (OK)
                                {
                                    IBR_HintManager::SetHint(u8"载入成功！", HintStayTimeMillis);
                                    IBF_Inst_Project.CurrentProjectRID = GlobalRnd();
                                    IsProjectOpen = true;
                                }
                                else IBR_HintManager::SetHint(u8"载入失败！", HintStayTimeMillis);
                            }, nullptr }); });
                }
                else { IBR_HintManager::SetHint(u8"已取消", HintStayTimeMillis); IBR_PopupManager::ClearPopupDelayed(); }
            });
    }
    void _IN_RENDER_THREAD OpenRecentAction(const std::wstring& Path)
    {
        SetWaitingPopup();

        IBR_RecentManager::Push(Path);
        IBR_RecentManager::Save();
        IBS_Push([=]()
            {
                Load(Path, [](bool OK) {IBRF_CoreBump.SendToR({ [=]()
                    {
                        IBR_PopupManager::ClearCurrentPopup();
                        if (OK)
                            {
                                IBR_HintManager::SetHint(u8"载入成功！", HintStayTimeMillis);
                                IBF_Inst_Project.CurrentProjectRID = GlobalRnd();
                                IsProjectOpen = true;
                            }
                        else
                        {
                            IBR_HintManager::SetHint(u8"载入失败！", HintStayTimeMillis);
                            IBR_RecentManager::WanDuZiLe();
                        }
                    }, nullptr }); });
            });
    }
    void _IN_RENDER_THREAD CreateAction()
    {
        Create();
    }

    void _IN_RENDER_THREAD CloseAction()
    {

        if (!IBF_Inst_Project.Project.ChangeAfterSave)
        {
            IBRF_CoreBump.SendToR({ [=]() {Close(); } });
            return;
        }
        AskIfSave([](bool OK)
            {
                if (OK)
                {
                    if (!IBF_Inst_Project.Project.IsNewlyCreated)
                    {
                        SetWaitingPopup();
                        IBS_Push([]()
                            {
                                Save(IBF_Inst_Project.Project.Path, [](bool OK2) 
                                    {
                                        IBR_PopupManager::ClearCurrentPopup();
                                        if (OK2)
                                        {
                                            Close();
                                            IBR_HintManager::SetHint(u8"关闭成功！", HintStayTimeMillis);
                                        }
                                        else IBR_HintManager::SetHint(u8"保存失败！关闭失败！", HintStayTimeMillis);
                                    });
                            });
                    }
                    else
                    {
                        SetWaitingPopup();
                        AskSavePath([](auto ws)
                            {
                                if (ws)Save(ws.value(), [](bool OK2) 
                                    {
                                        IBR_PopupManager::ClearCurrentPopup();
                                        if (OK2)
                                        {
                                            Close();
                                            IBR_HintManager::SetHint(u8"关闭成功！", HintStayTimeMillis);
                                        }
                                        else IBR_HintManager::SetHint(u8"保存失败！关闭失败！", HintStayTimeMillis);
                                    });
                                else { IBR_HintManager::SetHint(u8"已取消", HintStayTimeMillis); IBR_PopupManager::ClearPopupDelayed(); }
                            });
                    }
                }

                else
                {
                    IBRF_CoreBump.SendToR({ [=]() {Close(); } });
                    IBR_HintManager::SetHint(u8"关闭成功！", HintStayTimeMillis);
                }
            });
    }
    void _IN_RENDER_THREAD SaveAction()
    {

        if (!IBF_Inst_Project.Project.ChangeAfterSave)return;
        SetWaitingPopup();
        IBS_Push([=]()
            {
                Save(IBF_Inst_Project.Project.Path, [](bool OK) {IBRF_CoreBump.SendToR({ [=]()
                {
                    IBR_PopupManager::ClearCurrentPopup();
                    if (OK)IBR_HintManager::SetHint(u8"保存成功！", HintStayTimeMillis);
                    else IBR_HintManager::SetHint(u8"保存失败！", HintStayTimeMillis);
                }, nullptr }); });
            });

    }
    void _IN_RENDER_THREAD SaveAsAction()
    {
        SetWaitingPopup();
        AskSavePath([](auto ws)
            {
                if (ws)Save(ws.value(), [ws](bool OK) {IBRF_CoreBump.SendToR({ [=]()
                    {
                        IBR_PopupManager::ClearCurrentPopup();
                        if (OK)
                        {
                            IBF_Inst_Project.Project.Path = ws.value();
                            IBF_Inst_Project.Project.ProjName = FileName(ws.value());
                            IBR_HintManager::SetHint(u8"保存成功！", HintStayTimeMillis);
                        }
                        else IBR_HintManager::SetHint(u8"保存失败！", HintStayTimeMillis);
                    }, nullptr }); });
                else { IBR_HintManager::SetHint(u8"已取消", HintStayTimeMillis); IBR_PopupManager::ClearPopupDelayed(); }
            });
    }
    void _IN_RENDER_THREAD SaveOptAction()
    {
        if (IBF_Inst_Project.Project.IsNewlyCreated)SaveAsAction();
        else SaveAction();
    }
    void _IN_RENDER_THREAD OutputAction()
    {
        struct IniNameInput
        {
            bool Warning{ false };
            std::string Name;
            std::shared_ptr<BufString> Buf;
            IniNameInput() :Buf(new BufString) {}
        };

        std::vector<IniNameInput> Inis;
        std::shared_ptr<BufString> PathBuffer{ new BufString{} };
        std::shared_ptr<bool> OK{ new bool(false) };
        std::wstring WP;
        {
            IBD_RInterruptF(x);
            Inis.resize(IBF_Inst_Project.Project.Inis.size());
            auto V = IBF_Inst_Project.Project.ProjName;
            auto S = V.find_last_of(L'.');
            if (S != std::wstring::npos)
            {
                V = V.substr(0, S);
            }
            auto U = UnicodetoUTF8(V);
            
            for (size_t i = 0; i < Inis.size(); i++)
            {
                Inis[i].Name = IBF_Inst_Project.Project.Inis[i].Name;
                strcpy(Inis[i].Buf.get(), (U + "_" + IBF_Inst_Project.Project.Inis[i].Name + ".ini").c_str());
            }

            if(!IBF_Inst_Project.Project.LastOutputDir.empty())
                WP = IBF_Inst_Project.Project.LastOutputDir;
            else if (!IBF_Inst_Project.Project.Path.empty())
                WP = RemoveSpec(IBF_Inst_Project.Project.Path);

            strcpy(PathBuffer.get(), UnicodetoUTF8(WP).c_str());
            *OK = IsExistingDir(WP.c_str());
        }
        auto PF{ []() {IBR_HintManager::SetHint(u8"已取消导出。",HintStayTimeMillis); } };
        IBR_PopupManager::SetCurrentPopup(std::move(IBR_PopupManager::Popup{}.CreateModal(u8"导出项目", true, PF)
            .SetFlag(ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize).PushMsgBack([=]() mutable
                {
                    ImGui::SetWindowSize({ FontHeight * 20.0f,FontHeight * 30.0f });
                    if (ImGui::InputText(u8"目标路径", PathBuffer.get(), MAX_STRING_LENGTH))
                    {
                        WP = UTF8toUnicode(PathBuffer.get());
                        *OK = IsExistingDir(WP.c_str());
                        for (auto& I : Inis)I.Warning= PathFileExistsW((WP + UTF8toUnicode(I.Buf.get())).c_str());
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("..."))
                    {
                        auto Ret = InsertLoad::SelectFolderName(MainWindowHandle,InsertLoad::SelectFileType{ WP ,L"浏览", L"", L""});
                        if (Ret.Success)
                        {
                            WP = Ret.RetBuf;
                            strcpy(PathBuffer.get(), UnicodetoUTF8(WP).c_str());
                            *OK = IsExistingDir(WP.c_str());
                            for (auto& I : Inis)I.Warning = PathFileExistsW((WP + L"\\" + UTF8toUnicode(I.Buf.get())).c_str());
                        }
                    }

                    for (auto& I : Inis)
                    {
                        if (I.Name == LinkGroup_IniName)continue; //不显示这个，因为不导出
                        ImGui::InputText(("##" + I.Name).c_str(), I.Buf.get(), MAX_STRING_LENGTH);
                        I.Warning = PathFileExistsW((WP + L"\\" + UTF8toUnicode(I.Buf.get())).c_str());//Refresh per Frame
                        if (I.Warning)ImGui::Text(u8"警告：将会覆盖已有文件。");
                    }

                    if (*OK && !WP.empty())
                    {
                        if (ImGui::Button(u8"确定"))
                        {
                            auto V = GetIgnoredSection();
                            std::vector<std::wstring> TgPath;
                            TgPath.reserve(Inis.size());
                            OutputComplete = false;
                            for (auto& I : Inis)TgPath.push_back(WP + L"\\" + UTF8toUnicode(I.Buf.get()));
                            IBRF_CoreBump.SendToF([=] {
                                IBF_Inst_Setting.OutputDir() = UnicodetoUTF8(WP);
                                IBF_Inst_Setting.SaveSetting(IBR_Inst_Setting.SettingName);
                                if (IBF_Inst_Project.Project.LastOutputDir != WP)
                                {
                                    IBF_Inst_Project.Project.LastOutputDir = WP;
                                    IBF_Inst_Project.Project.ChangeAfterSave = true;
                                }
                                Output(WP, TgPath, V, true); });
                            IBRF_CoreBump.SendToR({ [] { IBR_PopupManager::ClearCurrentPopup(); if(!OutputComplete)SetWaitingPopup(); } });
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled(u8"确定");
                        ImGui::SameLine();
                        if (WP.empty())
                        {
                            ImGui::TextColored(IBR_Color::ErrorTextColor, u8"路径不可为空");
                            ImGui::SameLine();
                        }
                        if (!*OK)
                        {
                            ImGui::TextColored(IBR_Color::ErrorTextColor, u8"非法的模块路径");
                            ImGui::SameLine();
                        }
                    }
                })));
    }
    void _IN_RENDER_THREAD AutoOutputAction()
    {
        std::wstring WP;
        if (!IBF_Inst_Project.Project.LastOutputDir.empty())
            WP = IBF_Inst_Project.Project.LastOutputDir;
        else if (!IBF_Inst_Project.Project.Path.empty())
            WP = RemoveSpec(IBF_Inst_Project.Project.Path);
        else
        {
            WP = CurrentDirW;
            if (WP.back() == L'\\')WP.pop_back();
        }
        std::vector<std::wstring> TgPath;
        TgPath.reserve(IBF_Inst_Project.Project.Inis.size());
        OutputComplete = false;
        for (auto& I : IBF_Inst_Project.Project.Inis)
            TgPath.push_back(WP + L"\\" + IBF_Inst_Project.Project.ProjName + L"_" + UTF8toUnicode(I.Name) + L".ini");
        IBRF_CoreBump.SendToF([=] {Output(WP, TgPath, GetIgnoredSection(), true); });
    }

    void _IN_RENDER_THREAD ProjOpen_CreateAction()
    {
        IBRF_CoreBump.SendToR({ []() {CreateAction(); }, &ActionAfterClose });
        CloseAction();
    }
    void _IN_RENDER_THREAD ProjOpen_OpenAction()
    {
        IBRF_CoreBump.SendToR({ []() {OpenAction(); }, &ActionAfterClose });
        CloseAction();
    }
    void _IN_RENDER_THREAD ProjOpen_OpenRecentAction(const std::wstring& Path)
    {
        IBRF_CoreBump.SendToR({ [=]() {OpenRecentAction(Path); }, &ActionAfterClose });
        CloseAction();
    }

    void _IN_RENDER_THREAD OpenRecentOptAction(const std::wstring& Path)
    {
        if (IsOpen())ProjOpen_OpenRecentAction(Path);
        else OpenRecentAction(Path);
    }

    void _IN_RENDER_THREAD OnDropFile(GLFWwindow* window, int argc,const _TEXT_UTF8 char** argv)
    {
        using namespace std::string_literals;
        ((void)window);
        if (IBR_PopupManager::HasPopup)return;
        if (argc == 0)return;
        if (argc == 1)
        {
            std::string ext = ExtName(argv[0]);
            for (auto& c : ext)c = (char)toupper(c);
            if (ext == ExtensionNameC)
            {
                ProjOpen_OpenRecentAction(UTF8toUnicode(argv[0]));
                return;
            }
        }


        struct SHPSolution
        {
            std::string Name;
            int Type{ 0 };
        };
        std::vector<SHPSolution> Shapes;
        for (int i = 0; i < argc; i++)
        {
            //get extention name from argv[i] (UTF-8 encoding)
            //get file name without extension from argv[i] (UTF-8 encoding)

            std::string s = FileName(argv[i]);
            for (auto& c : s)c = (char)toupper(c);
            std::string name = FileNameNoExt(s);
            std::string ext = ExtName(s);
            if (ext == "VXL")
            {
                auto pVxl = IBB_ModuleAltDefault::DefaultArt_Voxel();
                if (!pVxl)
                    IBR_PopupManager::SetCurrentPopup(std::move(IBR_PopupManager::MessageModal(
                    u8"模块创建错误", u8"缺少默认VXL模块！",
                    { FontHeight * 10.0f, FontHeight * 7.0f }, false, true)));
                else if(IBR_Inst_Project.HasSection({ pVxl->GetFirstINI(), name }))
                    IBR_PopupManager::SetCurrentPopup(std::move(IBR_PopupManager::MessageModal(
                    u8"模块创建错误", u8"图像模块名称不可重复！",
                    { FontHeight * 10.0f, FontHeight * 7.0f }, false, true)));
                else IBR_Inst_Project.AddModule(*pVxl, name);
            }
            else if (ext == "SHP")
            {
                Shapes.push_back({ name,0 });
            }
            else if (ext == "INI")
            {
                IBB_ModuleAlt M;
                M.LoadFromFile(UTF8toUnicode(argv[i]).c_str());
                if (M.Available)
                {
                    IBR_Inst_Project.AddModule(M, GenerateModuleTag());
                    IBR_HintManager::SetHint(u8"模块创建成功！", HintStayTimeMillis);
                }
                else
                    IBR_PopupManager::SetCurrentPopup(std::move(IBR_PopupManager::MessageModal(
                        u8"模块创建错误", u8"该INI不是模块文件" + name,
                        { FontHeight * 10.0f, FontHeight * 7.0f }, false, true)));
            }
            else
            {
                IBR_PopupManager::SetCurrentPopup(std::move(IBR_PopupManager::MessageModal(
                    u8"模块创建错误", u8"不支持的文件类型 " + ext,
                    { FontHeight * 10.0f, FontHeight * 7.0f }, false, true)));
            }
        }
        if (!Shapes.empty())
        IBR_PopupManager::SetCurrentPopup(std::move(IBR_PopupManager::Popup{}
            .CreateModal(u8"导入文件", false)
            .SetFlag(ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize)
            .SetSize({ FontHeight * 20.0f, FontHeight * 6.0f + std::min(Shapes.size(), size_t(6)) * FontHeight * 2.0f })
            .PushMsgBack([SHP=std::move(Shapes)]() mutable
                {
                    ImGui::Text(u8"导入的SHP文件：");

                    auto CreateError = [](const char* Msg)
                        {
                            IBRF_CoreBump.SendToR({ [Msg]
                                {
                                    IBR_PopupManager::ClearCurrentPopup();
                                    IBR_PopupManager::SetCurrentPopup(std::move(IBR_PopupManager::MessageModal(
                                    u8"模块创建错误", Msg,
                                    { FontHeight * 10.0f, FontHeight * 7.0f }, false, true)));
                                } });
                        };

                    //give it up and use scroll bar instead
                    //IBR_ListMenu<SHPSolution> { SHP, u8"SHPSEL",  SelPolicy }.RenderUI();
                    for (size_t Index = 0; Index < SHP.size(); Index++)
                    {
                        auto& S = SHP[Index];
                        ImGui::Text(S.Name.c_str());
                        if (ImGui::RadioButton(std::format(u8"动画##{}", Index).c_str(), S.Type == 0))S.Type = 0;
                        ImGui::SameLine();
                        if (ImGui::RadioButton(std::format(u8"建筑##{}", Index).c_str(), S.Type == 1))S.Type = 1;
                        ImGui::SameLine();
                        if (ImGui::RadioButton(std::format(u8"步兵##{}", Index).c_str(), S.Type == 2))S.Type = 2;
                        ImGui::SameLine();
                        if (ImGui::RadioButton(std::format(u8"车辆##{}", Index).c_str(), S.Type == 3))S.Type = 3;
                    }

                    if (ImGui::Button(u8"确定"))
                    {
                        for (auto& S : SHP)
                        {
                            switch (S.Type)
                            {
                            case 0://Animation
                            {
                                auto pShp = IBB_ModuleAltDefault::DefaultArt_Animation();
                                if (!pShp)
                                    CreateError(u8"缺少默认动画模块！");
                                else if(IBR_Inst_Project.HasSection({ pShp->GetFirstINI(), S.Name }))
                                    CreateError(u8"图像模块名称不可重复！");
                                else IBR_Inst_Project.AddModule(*pShp, S.Name);
                                break;
                            }
                            case 1://Building
                            {
                                auto pShp = IBB_ModuleAltDefault::DefaultArt_SHPBuilding();
                                if (!pShp)
                                    CreateError(u8"缺少默认建筑模块！");
                                else if (IBR_Inst_Project.HasSection({ pShp->GetFirstINI(), S.Name }))
                                    CreateError(u8"图像模块名称不可重复！");
                                else IBR_Inst_Project.AddModule(*pShp, S.Name);
                                break;
                            }
                            case 2://Infantry
                            {
                                auto pShp = IBB_ModuleAltDefault::DefaultArt_SHPInfantry();
                                if (!pShp)
                                    CreateError(u8"缺少默认步兵模块！");
                                else if (IBR_Inst_Project.HasSection({ pShp->GetFirstINI(), S.Name }))
                                    CreateError(u8"图像模块名称不可重复！");
                                else IBR_Inst_Project.AddModule(*pShp, S.Name);
                                break;
                            }
                            case 3://Vehicle
                            {
                                auto pShp = IBB_ModuleAltDefault::DefaultArt_SHPVehicle();
                                if (!pShp)
                                    CreateError(u8"缺少默认车辆模块！");
                                else if (IBR_Inst_Project.HasSection({ pShp->GetFirstINI(), S.Name }))
                                    CreateError(u8"图像模块名称不可重复！");
                                else IBR_Inst_Project.AddModule(*pShp, S.Name);
                                break;
                            }
                            default:break;
                            }
                        }
                        IBR_PopupManager::ClearPopupDelayed();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(u8"取消"))
                        IBR_PopupManager::ClearPopupDelayed();
                })));
    }
    void _IN_RENDER_THREAD ProjActionByKey()
    {
        //Ctrl + Shift + S = Save As
        if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            SaveAsAction();
        }
        //Ctrl + S = Save
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        {
            SaveOptAction();
        }
        //Ctrl + O = Open
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
        {
            ProjOpen_OpenAction();
        }
        //Ctrl + W = Close
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W))
        {
            ProjOpen_CreateAction();
        }
        //Ctrl + E = Export
        else if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E))
        {
            OutputAction();
        }
    }
};
