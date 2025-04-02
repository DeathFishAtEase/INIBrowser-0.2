#include "IBRender.h"
#include "IBFront.h"
#include "Global.h"
#include "FromEngine/RFBump.h"
#include "FromEngine/global_timer.h"
#include<imgui_internal.h>


void IBR_MainMenu::RenderUIBar()
{
    ImGui::PushStyleColor(ImGuiCol_Button, { 0,0,0,0 });
    for (size_t i = 0; i < ItemList.size(); i++)
    {
        if (ItemList[i].Button())
        {
            Choice = i;
            IBR_UICondition::MenuChangeShow = true;
            if (EnableLog)
            {
                GlobalLog.AddLog_CurTime(false);
                GlobalLog.AddLog("�л������˵�");
            }
        }
        ImGui::SameLine();
    }
    ImGui::PopStyleColor(1);
}
void IBR_MainMenu::RenderUIMenu()
{
    sprintf(LogBuf, "%u", Choice);
    if (Choice < ItemList.size())ItemList.at(Choice).Menu();
}
void IBR_MainMenu::ChooseMenu(size_t S)
{
    if (S >= 0 && S < ItemList.size())Choice = S;
}

void ControlPanel_Debug()
{
    ImGui::Text(u8"������Ϣ��");

    IBR_Inst_Debug.RenderUI();

    int ii = 0;
    for (auto& s : IBG_Undo.Stack)
    {
        ImGui::Text(s.Id.c_str());
        if (ii == IBG_Undo.Cursor)ImGui::Separator();
        ++ii;
    }

}

void ControlPanel_About();

void ControlPanel_WaitOpen()
{
    ImGui::TextDisabled(u8"��ȴ�����Ŀ");
}

void ControlPanel_File()
{
    if (ImGui::Button(u8"����"))IBR_ProjectManager::SaveOptAction();//
    if (ImGui::Button(u8"���Ϊ"))IBR_ProjectManager::SaveAsAction();//
    if (IBR_Inst_Project.IBR_SectionMap.empty())ImGui::TextDisabled(u8"����");
    else if (ImGui::Button(u8"����"))IBR_ProjectManager::OutputAction();//

    ImGui::NewLine();
    if (ImGui::Button(u8"�ر���Ŀ"))IBR_ProjectManager::ProjOpen_CreateAction(); //���·����Ϊʲô��CreateAction
    if (ImGui::Button(u8"����Ŀ"))IBR_ProjectManager::ProjOpen_OpenAction();//
    ImGui::NewLine();
    IBR_RecentManager::RenderUI();//
    /*
    if (!IsProjectOpen)
    {
        if (ImGui::Button(u8"�½�"))IBR_ProjectManager::CreateAction();//
        if (ImGui::Button(u8"��"))IBR_ProjectManager::OpenAction();//
        ImGui::NewLine();
        IBR_RecentManager::RenderUI();//
    }
    else
    {
        if (ImGui::Button(u8"����"))IBR_ProjectManager::SaveOptAction();//
        if (ImGui::Button(u8"���Ϊ"))IBR_ProjectManager::SaveAsAction();//
        if (IBR_Inst_Project.IBR_SectionMap.empty())ImGui::TextDisabled(u8"����");
        else if (ImGui::Button(u8"����"))IBR_ProjectManager::OutputAction();//

        ImGui::NewLine();
        if (ImGui::Button(u8"�ر�"))IBR_ProjectManager::ProjOpen_CreateAction(); //���·����Ϊʲô��CreateAction
        //IBR_ProjectManager::CloseAction();//

        ImGui::NewLine();
        //if (ImGui::Button(u8"�½�"))IBR_ProjectManager::ProjOpen_CreateAction();//
        if (ImGui::Button(u8"��"))IBR_ProjectManager::ProjOpen_OpenAction();//
        ImGui::NewLine();
        IBR_RecentManager::RenderUI();//
    }
    */
}

void ControlPanel_View()
{
    if (!IBR_ProjectManager::IsOpen())
    {
        ControlPanel_WaitOpen();
        return;
    }
    IBR_FullView::RenderUI();
}



void ControlPanel_ListView()
{
    if (!IBR_ProjectManager::IsOpen())
    {
        ControlPanel_WaitOpen();
        return;
    }
    {
        int SelectN = 0;
        bool FullSelected = true;

        IBD_RInterruptF(x);

        for (auto& ini : IBF_Inst_Project.Project.Inis)
        {
            if (ini.Secs_ByName.empty())continue;
            for (auto& sec : ini.Secs)
            {
                if (sec.second.Dynamic.Selected)++SelectN;
                else FullSelected = false;
            }
        }

        bool SelectAll{ false }, SelectNone{ false }, Delete{ false }, Duplicate{ false };
        if (FullSelected)
        {
            if (SelectN == 0)ImGui::TextDisabled(u8"ȫѡ");
            else if (ImGui::Button(u8"ȫ��ѡ"))SelectNone = true;
        }
        else if (ImGui::Button(u8"ȫѡ"))SelectAll = true;
        ImGui::SameLine();
        if (SelectN == 0)
        {
            ImGui::TextDisabled(u8"ɾ��"); ImGui::SameLine();
            ImGui::TextDisabled(u8"����");
        }
        else
        {
            if (ImGui::Button(u8"ɾ��"))Delete = true; ImGui::SameLine();
            if (ImGui::Button(u8"����"))Duplicate = true;
        }

        if (SelectAll || SelectNone || Delete || Duplicate)
        {
            if (Duplicate)IBR_Inst_Project.CopyTransform.clear();
            for (auto& ini : IBF_Inst_Project.Project.Inis)
            {
                if (ini.Secs_ByName.empty())continue;
                if (Duplicate)
                {
                    for (auto& sec : ini.Secs)
                    {
                        if (sec.second.Dynamic.Selected && Duplicate)
                            IBR_Inst_Project.CopyTransform[sec.second.Name] = GenerateModuleTag();
                    }
                }
                for (auto& sec : ini.Secs)
                {
                    if (SelectAll)sec.second.Dynamic.Selected = true;
                    if (SelectNone)sec.second.Dynamic.Selected = false;
                    if (sec.second.Dynamic.Selected && Delete)IBRF_CoreBump.SendToR({ [=]() {IBR_Inst_Project.DeleteSection({ ini.Name,sec.second.Name }); },nullptr });
                    if (sec.second.Dynamic.Selected && Duplicate)
                        IBRF_CoreBump.SendToR({ [=]()
                            {
                                IBB_Section_Desc desc = { ini.Name,IBR_Inst_Project.CopyTransform[sec.second.Name]};
                                IBR_Inst_Project.GetSection({ ini.Name,sec.second.Name }).DuplicateSection(desc);
                                auto rsc = IBR_Inst_Project.GetSection(desc);
                                auto rsc_orig = IBR_Inst_Project.GetSection({ ini.Name,sec.second.Name });
                                auto& rsd = *rsc.GetSectionData();
                                auto& rsd_orig = *rsc_orig.GetSectionData();
                                rsd.RenameDisplayImpl(rsd_orig.DisplayName);
                                rsd.EqPos = rsd_orig.EqPos + dImVec2{2.0 * FontHeight, 2.0 * FontHeight};
                                rsd.EqSize = rsd_orig.EqSize;
                                rsd.Dragging = true;
                                rsd.IsComment = rsd_orig.IsComment;
                                if (rsd.IsComment)
                                {
                                    rsd.CommentEdit = std::make_shared<BufString>();
                                    strcpy(rsd.CommentEdit.get(), rsd_orig.CommentEdit.get());
                                }
                            },nullptr });
                    //��V0.2.0�����嵥���ģ���75�����漰�ֶ���Ŀ�仯��ָ��Ӧ����IBF_SendToR��������ѭ����ͷ��
                }
            }
            if (Duplicate)
                IBRF_CoreBump.SendToR({ [=]() {IBF_Inst_Project.UpdateAll(); IBR_WorkSpace::HoldingModules = true;  },nullptr });
            else IBRF_CoreBump.SendToR({ [=]() {IBF_Inst_Project.UpdateAll(); },nullptr });
        }

        for (auto& ini : IBF_Inst_Project.Project.Inis)
        {
            if (ini.Secs_ByName.empty())continue;//���ο�INI
            //if (ImGui::TreeNode(ini.Name == LinkGroup_IniName ? u8"������" : (MBCStoUTF8(ini.Name).c_str())))
            {
                for (auto& sec : ini.Secs)
                {
                    auto rsc = IBR_Inst_Project.GetSection({ ini.Name,sec.second.Name });
                    ImGui::Checkbox((u8"##" + sec.second.Name).c_str(), &sec.second.Dynamic.Selected);
                    ImGui::SameLine();
                    ImGui::Text(rsc.GetSectionData()->DisplayName.c_str());
                    ImGui::SameLine();
                    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - FontHeight * 2.0f));//4.5���ַ����Ҳ����ݵ�Ԥ���ռ�
                    if (ImGui::ArrowButton((sec.second.Name + "_ub_arr").c_str(), ImGuiDir_Right))
                    {
                        //auto rsc = IBR_Inst_Project.GetSection(IBB_Section_Desc{ ini.Name,sec.second.Name });
                        auto dat = rsc.GetSectionData();
                        if (dat != nullptr)
                        {
                            IBR_EditFrame::SetActive(rsc.ID);
                            IBR_FullView::EqCenter = dat->EqPos + (dat->EqSize / 2.0);
                        }
                    }
                }
                //ImGui::TreePop();
            }
        }
    }
}



void ControlPanel_Edit()
{
    if (!IBR_ProjectManager::IsOpen())
    {
        ControlPanel_WaitOpen();
        return;
    }
    if (IBR_EditFrame::CurSection.ID != IBR_EditFrame::PrevId)
    {
        IBR_EditFrame::PrevId = IBR_EditFrame::CurSection.ID;
        IBR_EditFrame::UpdateSection();
    }
    if (!IBR_EditFrame::Empty)if (!IBR_EditFrame::CurSection.HasBack())
    {
        IBR_EditFrame::Empty = true;
    }
    IBR_EditFrame::RenderUI();
}

bool ImGui_TextDisabled_Helper(const char* Text)
{
    ImGui::TextDisabled(Text); return false;
}
bool SmallButton_Disabled_Helper(bool cond, const char* Text)
{
    return cond ? ImGui::SmallButton(Text) : ImGui_TextDisabled_Helper(Text);
}

/*
#define MenuItemID_FILE 0
#define MenuItemID_VIEW 1
#define MenuItemID_LIST 2
//#define MenuItemID_MODULE 3
#define MenuItemID_EDIT 4
#define MenuItemID_SETTING 5
#define MenuItemID_ABOUT 6
#define MenuItemID_DEBUG 7
*/
IBR_MainMenu IBR_Inst_Menu
{
{
    {[]() {return ImGui::SmallButton(u8"�ļ�"); },ControlPanel_File},
    {[]() {return SmallButton_Disabled_Helper(IBR_ProjectManager::IsOpen(), u8"��ͼ"); },ControlPanel_View},
    {[]() {return SmallButton_Disabled_Helper(IBR_ProjectManager::IsOpen(), u8"�б�"); },ControlPanel_ListView},
    //{[]() {return false;/*SmallButton_Disabled_Helper(IsProjectOpen, u8"Ԥ��");*/ },ControlPanel_Module},
    {[]() {return SmallButton_Disabled_Helper(IBR_ProjectManager::IsOpen(), u8"�༭"); },ControlPanel_Edit},
    {[]() {return ImGui::SmallButton(u8"����"); },[]() {IBR_Inst_Setting.RenderUI(); }},
    {[]() {return ImGui::SmallButton(u8"����"); },ControlPanel_About},
    {[]() {return ImGui::SmallButton(u8"����"); },ControlPanel_Debug},
}
};


void ControlPanel_About()
{
    ImGui::TextWrapped(u8"INI����� V%s", Version.c_str());
    ImGui::TextWrapped(u8"��������� 2022��11��");
    ImGui::Separator();
    ImGui::TextWrapped(u8"���ߣ�std::iron_hammer");
    ImGui::TextWrapped(u8"QQ������֮����2482911962��");
    ImGui::TextWrapped(u8"���ɣ���030504");
    ImGui::TextWrapped(u8"���䣺2482911962@qq.com");
    ImGui::Separator();
    ImGui::TextWrapped(u8"�������ӣ�");
    ImGui::TextWrapped(u8"����������ɫ����ɣ�"); //ImGui::SameLine();
    if (ImGui::Button(u8"��������##A"))
    {
        ImGui::LogToClipboard();
        ImGui::LogText("https://tieba.baidu.com/p/8005920823");
        ImGui::LogFinish();
    }ImGui::SameLine();
    if (ImGui::Button(u8"������##A"))
    {
        ::ShellExecuteA(nullptr, "open", "https://tieba.baidu.com/p/8005920823", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::TextWrapped(u8"�������������ս�ɣ�"); //ImGui::SameLine();
    if (ImGui::Button(u8"��������##B"))
    {
        ImGui::LogToClipboard();
        ImGui::LogText("https://tieba.baidu.com/p/8005924464");
        ImGui::LogFinish();
    }ImGui::SameLine();
    if (ImGui::Button(u8"������##B"))
    {
        ::ShellExecuteA(nullptr, "open", "https://tieba.baidu.com/p/8005924464", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::TextWrapped(u8"�������������ս�3ini�ɣ�"); //ImGui::SameLine();
    if (ImGui::Button(u8"��������##C"))
    {
        ImGui::LogToClipboard();
        ImGui::LogText("https://tieba.baidu.com/p/8133473361");
        ImGui::LogFinish();
    }ImGui::SameLine();
    if (ImGui::Button(u8"������##C"))
    {
        ::ShellExecuteA(nullptr, "open", "https://tieba.baidu.com/p/8133473361", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::TextWrapped(u8"ע������������ͬ������");
    ImGui::TextWrapped(u8"ȫ���汾���أ��ٶ����̣�"); //ImGui::SameLine();
    if (ImGui::Button(u8"��������##D"))
    {
        ImGui::LogToClipboard();
        ImGui::LogText("https://pan.baidu.com/s/1EpzAuIQfbU1-7sjb2YJocg?pwd=EASB");
        ImGui::LogFinish();
    }ImGui::SameLine();
    if (ImGui::Button(u8"������##D"))
    {
        ::ShellExecuteA(nullptr, "open", "https://pan.baidu.com/s/1EpzAuIQfbU1-7sjb2YJocg?pwd=EASB", NULL, NULL, SW_SHOWNORMAL);
    }
    ImGui::TextWrapped(u8"��ȡ�룺EASB");
}
