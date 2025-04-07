
#include "IBSave.h"
#include "IBRender.h"
#include "Global.h"
#include <shlwapi.h>
#include ".\FromEngine\global_tool_func.h"
extern bool RefreshLangBuffer1;

namespace InsertLoad
{
    LONG g_lOriWndProc = NULL;
#define  ID_COMBO_ADDR 0x47c
#define  ID_LEFT_TOOBAR 0x4A0
    LRESULT static __stdcall  _WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_COMMAND:
        {
            if (wParam == IDOK)
            {
                wchar_t wcDirPath[MAX_PATH] = { 0 };
                HWND hComboAddr = GetDlgItem(hwnd, ID_COMBO_ADDR);
                if (hComboAddr != NULL)
                {
                    GetWindowText(hComboAddr, wcDirPath, MAX_PATH);
                }
                if (!wcslen(wcDirPath))
                {
                    break;
                }
                DWORD dwAttr = GetFileAttributes(wcDirPath);
                if (dwAttr != -1 && (FILE_ATTRIBUTE_DIRECTORY & dwAttr))
                {
                    LPOPENFILENAMEW oFn = (LPOPENFILENAME)GetProp(hwnd, L"OPENFILENAME");
                    if (oFn)
                    {
                        int size = oFn->nMaxFile > MAX_PATH ? MAX_PATH : oFn->nMaxFile;
                        memcpy(oFn->lpstrFile, wcDirPath, size * sizeof(wchar_t));
                        RemoveProp(hwnd, L"OPENFILENAME");
                        EndDialog(hwnd, 1);
                    }
                    else
                    {
                        EndDialog(hwnd, 0);
                    }
                }
                break;
            }
            //////////////////////////////////////////////////////////////////////////
            //��������toolbar������WM_COMMOND��Ϣ���������ߵ�toolbar��, �����OK��ť�Ե���Ͽ�
            HWND hCtrl = (HWND)lParam;
            if (hCtrl == NULL)
            {
                break;
            }
            int ctrlId = GetDlgCtrlID(hCtrl);
            if (ctrlId == ID_LEFT_TOOBAR)
            {
                HWND hComboAddr = GetDlgItem(hwnd, ID_COMBO_ADDR);
                if (hComboAddr != NULL)
                {
                    SetWindowTextW(hComboAddr, L"");
                }
            }
        }
        break;
        }
        int i = CallWindowProc((WNDPROC)g_lOriWndProc, hwnd, uMsg, wParam, lParam);
        return i;
    }
    UINT_PTR static __stdcall  MyFolderProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam)
    {
        //�ο�reactos��֪��hdlg ��һ�����صĶԻ����丸����Ϊ���ļ��Ի��� OK��CANCEL��ť�ȿؼ�����Ϣ�ڸ����ڴ���
        ((void)wParam);
        if (uiMsg == WM_NOTIFY)
        {
            LPOFNOTIFY lpOfNotify = (LPOFNOTIFY)lParam;
            if (lpOfNotify->hdr.code == CDN_INITDONE)
            {
                SetPropW(GetParent(hdlg), L"OPENFILENAME", (HANDLE)(lpOfNotify->lpOFN));
                g_lOriWndProc = ::SetWindowLongW(::GetParent(hdlg), GWL_WNDPROC, (LONG)_WndProc);
            }
            if (lpOfNotify->hdr.code == CDN_SELCHANGE)
            {
                wchar_t wcDirPath[MAX_PATH] = { 0 };
                CommDlg_OpenSave_GetFilePathW(GetParent(hdlg), wcDirPath, sizeof(wcDirPath));
                HWND hComboAddr = GetDlgItem(GetParent(hdlg), ID_COMBO_ADDR);
                if (hComboAddr != NULL)
                {
                    if (wcslen(wcDirPath))
                    {
                        //ȥ���ļ��п�ݷ�ʽ�ĺ�׺����
                        int pathSize = wcslen(wcDirPath);
                        if (pathSize >= 4)
                        {
                            wchar_t* wcExtension = PathFindExtensionW(wcDirPath);
                            if (wcslen(wcExtension))
                            {
                                wcExtension = CharLowerW(wcExtension);
                                if (!wcscmp(wcExtension, L".lnk"))
                                {
                                    wcDirPath[pathSize - 4] = L'\0';
                                }
                            }
                        }

                        SetWindowTextW(hComboAddr, wcDirPath);
                    }
                    else
                    {
                        SetWindowTextW(hComboAddr, L"");
                    }
                }
            }
        }
        return 1;
    }

    SelectFileRet SelectFileName(HWND Root, const SelectFileType& Type, BOOL(_stdcall* Proc)(LPOPENFILENAMEW), bool UseFolder)
    {
        OPENFILENAMEW ofn;
        WCHAR szFile[296]{};
        StrCpyW(szFile, Type.InitialFileName.c_str());

        static std::wstring FilterStr;
        if (FilterStr.empty() || RefreshLangBuffer1)
        {
            auto T4 = locw("GUI_OutputModule_Type4");
            FilterStr.resize(T4.size() + 16);
            wcscpy(FilterStr.data(), T4.c_str());
            wcscpy(FilterStr.data() + T4.size() + 1 , L"..");
            RefreshLangBuffer1 = false;
        }

        std::wstring wptitle = Type.Title;

        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = Root;
        ofn.lpstrFilter = Type.Filter; //"��������(*_pack.dat)\0*_pack.dat\0�����ļ� (*.*)\0*.*\0\0"
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFileTitle = wptitle.data();//"ѡ��Ҫ������ļ�"
        ofn.nMaxFileTitle = wptitle.size();
        ofn.lpstrInitialDir = Type.InitialPath.c_str();
        ofn.hInstance = (HMODULE)GetCurrentProcess();
        if (UseFolder)
        {
            ofn.lpfnHook = MyFolderProc;
            ofn.lpstrFilter = FilterStr.c_str();
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_ENABLEHOOK | OFN_HIDEREADONLY;
        }
        else ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_OVERWRITEPROMPT;

        SelectFileRet Ret;
        Ret.Success = (Proc(&ofn));
        if (Ret.Success)
        {
            if (szFile[0])
            {
                Ret.RetBuf = szFile;
                Ret.PathOffset = ofn.nFileOffset;
            }
        }
        else
        {
            Ret.RetBuf = L"";
            Ret.PathOffset = 0;
        }
        if (EnableLog)
        {
            GlobalLog.AddLog_CurTime(false);
            GlobalLog.AddLog(locc("Log_SelectFile"));
        }
        return Ret;
    }

}

std::atomic<bool> IBS_Suspended{ false };
InfoStack<StdMessage> SStack;
DWORD SThreadID;
SaveFile ProjSL;//Save & Load of a project
std::string ExtName(const std::string& ss)//��չ������'.' 
{
    using namespace std;
    auto p = ss.find_last_of('.');
    return p == ss.npos ? "" : string(ss.begin() + min(p + 1, ss.length()), ss.end());
}
std::string FileNameNoExt(const std::string& ss)//�ļ�������'.' 
{
    using namespace std;
    auto p = ss.find_last_of('.');
    return p == ss.npos ? ss : string(ss.begin(), ss.begin() + min(p, ss.length()));
}
std::string FileName(const std::string& ss)//�ļ���
{
    using namespace std;
    auto p = ss.find_last_of('\\');
    return p == ss.npos ? ss : string(ss.begin() + min(p + 1, ss.length()), ss.end());
}
std::wstring FileName(const std::wstring& ss)//�ļ���
{
    using namespace std;
    auto p = ss.find_last_of(L'\\');
    return p == ss.npos ? ss : wstring(ss.begin() + min(p + 1, ss.length()), ss.end());
}


WriteFileHeader IBS_SaveProject
{
    "Project" ,
     [](const ExtFileClass& File)-> bool
     {
        if (EnableLog)
        {
            GlobalLogB.AddLog_CurTime(false);
            GlobalLogB.AddLog(locc("Log_CallSaveAction"));
        }
        File.WriteData(IBS_Inst_Project.CreateTime);
        File.WriteData(IBS_Inst_Project.CreateVersionMajor);
        File.WriteData(IBS_Inst_Project.CreateVersionMinor);
        File.WriteData(IBS_Inst_Project.CreateVersionRelease);
        File.WriteData(IBS_Inst_Project.FullView_Ratio);
        File.WriteData(IBS_Inst_Project.FullView_EqCenter);
        File.WriteData(IBS_Inst_Project.LastOutputDir);
        File.WriteVector(IBS_Inst_Project.Data);
        return true;
     }
};
ReadFileHeader IBS_LoadProject
{
    "Project" ,
     [](const ExtFileClass& File,int FVersion,int Length)-> bool
    {
        if (EnableLog)
        {
            GlobalLogB.AddLog_CurTime(false);
            GlobalLogB.AddLog(locc("Log_CallLoadAction"));
        }
        (void)Length;
        if (FVersion > VersionN)
        {
            if (EnableLog)
            {
                GlobalLogB.AddLog_CurTime(false);
                GlobalLogB.AddLog((u8"IBS_LoadProject.Action �� " + loc("Log_FileVersionTooHigh")).c_str());
            }
            {
                auto VS = UTF8toUnicode(GetVersionStr(FVersion));
                MessageBoxW(MainWindowHandle, std::vformat(locw("Error_NeedHigherVersion"),
                    std::make_wformat_args(VS, FVersion)).c_str(), locwc("Error_LoadProjectFailed"), MB_OK | MB_ICONERROR);
            }
            return false;
        }
        else if (FVersion >= 203)
        {
            File.ReadData(IBS_Inst_Project.CreateTime);
            File.ReadData(IBS_Inst_Project.CreateVersionMajor);
            File.ReadData(IBS_Inst_Project.CreateVersionMinor);
            File.ReadData(IBS_Inst_Project.CreateVersionRelease);
            File.ReadData(IBS_Inst_Project.FullView_Ratio);
            File.ReadData(IBS_Inst_Project.FullView_EqCenter);
            File.ReadData(IBS_Inst_Project.LastOutputDir);
            File.ReadVector(IBS_Inst_Project.Data);
        }
        else//202 or lower
        {
            File.ReadData(IBS_Inst_Project.CreateTime);
            File.ReadData(IBS_Inst_Project.CreateVersionMajor);
            File.ReadData(IBS_Inst_Project.CreateVersionMinor);
            File.ReadData(IBS_Inst_Project.CreateVersionRelease);
            File.ReadData(IBS_Inst_Project.FullView_Ratio);
            File.ReadData(IBS_Inst_Project.FullView_EqCenter);
            File.ReadVector(IBS_Inst_Project.Data);
        }
        return true;
    }
};

void IBS_Thr_SaveLoop()
{
    SThreadID = GetCurrentThreadId();

    ProjSL.ReadBlockProcess[IBS_LoadProject.UsingID] = IBS_LoadProject;
    ProjSL.WriteBlockProcess.push_back(IBS_SaveProject);

    IBS_Complete();
    while (1)
    {
        for (const auto& msg : SStack.Release())
        {
            msg();
        }
        IBS_Complete();
    }
}

void IBS_Complete()
{
    if (!IBS_Suspended.load())
    {
        IBS_Suspended.store(true);
        IBG_SuspendThread(SThreadID);
    }
}

void IBS_Push(const StdMessage& Msg)
{
    SStack.Push(Msg);
    if (IBS_Suspended.load())
    {
        IBS_Suspended.store(false);
        IBG_ResumeThread(SThreadID);
    }
}

bool IBS_Project::Save()
{
    return ProjSL.Write(Path.c_str());
}
bool IBS_Project::Load()
{
    ProjName = FileName(Path);
    return ProjSL.Read(Path.c_str());
}

bool IBS_Project::Save(const std::wstring& _Path)
{
    Path = _Path;
    if (LastOutputDir.empty())
    {
        LastOutputDir = RemoveSpec(_Path);
        IBF_Inst_Project.Project.LastOutputDir = LastOutputDir;
    }
    bool R = ProjSL.Write(Path.c_str());
    if (IBF_Inst_Setting.OutputOnSave())
    {
        IBRF_CoreBump.SendToR({ [=]() {IBR_ProjectManager::AutoOutputAction(); }});
    }
    return R;
}
bool IBS_Project::Load(const std::wstring& _Path)
{
    Path = _Path;
    ProjName = FileName(Path);
    return ProjSL.Read(Path.c_str());
}



