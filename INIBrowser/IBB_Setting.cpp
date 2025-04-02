#include "FromEngine/Include.h"
#include "FromEngine/global_tool_func.h"
#include "IBBack.h"
#include "Global.h"


IBG_SettingPack GlobalSettingPack;


bool ReadSettingFileGen(const ExtFileClass& File, int Order);
bool WriteSettingFileGen(const ExtFileClass& File, int Order);

ReadFileHeader IBB_RSetting
{
    "IBB_Setting_F_200" ,
     [](const ExtFileClass& File,int FVersion,int Length)-> bool
    {
        if (EnableLog)
        {
            GlobalLogB.AddLog_CurTime(false);
            GlobalLogB.AddLog("��Setting.dat�������á�");
        }
        (void)Length;
        if (FVersion > 200)
        {
            return ReadSettingFileGen(File, 0);
        }
        return false;
    }
};

WriteFileHeader IBB_WSetting
{
    "IBB_Setting_F_200" ,
     [](const ExtFileClass& File)-> bool
     {
        /*
        if (EnableLog)
        {
            GlobalLogB.AddLog_CurTime(false);
            GlobalLogB.AddLog("������IBB_WSetting.Action��");
        }*/
        return WriteSettingFileGen(File, 0);
     }
};

ReadFileHeader IBB_RLastOutput
{
    "IBB_LastOutput_F_203" ,
     [](const ExtFileClass& File,int FVersion,int Length)-> bool
    {
        if (EnableLog)
        {
            GlobalLogB.AddLog_CurTime(false);
            GlobalLogB.AddLog("��Setting.dat�����ϴε���Ŀ¼��");
        }
        (void)Length;
        if (FVersion >= 203)
        {
            auto R = File.ReadData(IBF_Inst_Setting.List.LastOutputDir);
            if (EnableLog)
            {
                GlobalLogB.AddLog_CurTime(false);
                GlobalLogB.AddLog(("LastOutputDir = " + UTF8toMBCS(IBF_Inst_Setting.List.LastOutputDir)).c_str());
            }
            return R;
        }
        else
        {
            if (EnableLog)
            {
                GlobalLogB.AddLog_CurTime(false);
                GlobalLogB.AddLog("����Ŀ¼��İ汾���ͣ�����ʧ�ܡ�");
            }
            return false;
        }
    }
};

WriteFileHeader IBB_WLastOutput
{
    "IBB_LastOutput_F_203" ,
     [](const ExtFileClass& File)-> bool
     {
        return File.WriteData(IBF_Inst_Setting.List.LastOutputDir);
     }
};

const std::unordered_map<IBB_SettingType::_Type, std::function<bool(const IBB_SettingType&, const ExtFileClass&)>> ReadTypeMap =
{
    {IBB_SettingType::None,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        (void)Ty; (void)File; return true;
    }},
    {IBB_SettingType::IntA,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        return File.ReadData(*((int32_t*)Ty.Data));
    }},
    {IBB_SettingType::IntB,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        return File.ReadData(*((int32_t*)Ty.Data));
    }},
    {IBB_SettingType::Bool,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        return File.ReadData(*((bool*)Ty.Data));
    }},
};

const std::unordered_map<IBB_SettingType::_Type, std::function<bool(const IBB_SettingType&, const ExtFileClass&)>> WriteTypeMap =
{
    {IBB_SettingType::None,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        (void)Ty; (void)File; return true;
    }},
    {IBB_SettingType::IntA,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        return File.WriteData(*((int32_t*)Ty.Data));
    }},
    {IBB_SettingType::IntB,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        return File.WriteData(*((int32_t*)Ty.Data));
    }},
    {IBB_SettingType::Bool,[](const IBB_SettingType& Ty, const ExtFileClass& File) -> bool
    {
        return File.WriteData(*((bool*)Ty.Data));
    }},
};

extern std::vector<int> RW_ReadOrder;
extern std::vector<int> RW_WriteOrder;


void IBB_SettingRegisterRW(SaveFile& Save)
{
    Save.ReadBlockProcess[IBB_RSetting.UsingID] = IBB_RSetting;
    Save.WriteBlockProcess.push_back(IBB_WSetting);

    Save.ReadBlockProcess[IBB_RLastOutput.UsingID] = IBB_RLastOutput;
    Save.WriteBlockProcess.push_back(IBB_WLastOutput);
}

bool ReadSettingFileGen(const ExtFileClass& File, int Order)
{
    (void(Order));
    for (int Tg : RW_ReadOrder)
    {
        auto& Ty = IBF_Inst_Setting.List.Types[Tg];
        if (!ReadTypeMap.at(Ty.Type)(Ty, File))
        {
            return false;
        }
    }return true;
}
bool WriteSettingFileGen(const ExtFileClass& File, int Order)
{
    (void(Order));
    for (int Tg : RW_WriteOrder)
    {
        auto& Ty = IBF_Inst_Setting.List.Types[Tg];
        if (!WriteTypeMap.at(Ty.Type)(Ty, File))
        {
            return false;
        }
    }return true;
}


IBB_SettingTypeList::IBB_SettingTypeList()
{
    Types = {
        {
            IBB_SettingType::IntA,
                u8"֡������", u8"������������Ч\n����ʹ�õ�֡���Խ�ԼCPU\n��Χ15~2000\n����-1������\nĬ��25",
                (void*)&Pack.FrameRateLimit,
            {
                (const void*)&IBG_SettingPack::____FrameRateLimit_Def,
                (const void*)&IBG_SettingPack::____FrameRateLimit_Min,(const void*)&IBG_SettingPack::____FrameRateLimit_Max,
                (const void*)&IBG_SettingPack::____FrameRateLimit_SpV,(const void*)&IBG_SettingPack::____FrameRateLimit_SpV,
            }
        },
        {
            IBB_SettingType::IntA,
                u8"�����С",u8"������������Ч\nʹ�õ��ֺ�\n��Χ12~48\nĬ��24",
                (void*)&Pack.FontSize,
            {
                (const void*)&IBG_SettingPack::____FontSize_Def,
                (const void*)&IBG_SettingPack::____FontSize_Min,(const void*)&IBG_SettingPack::____FontSize_Max,
            }
        },
        {
            IBB_SettingType::IntA,
                u8"�˵�ÿҳ��Ŀ",u8"������������Ч\n��ҳ�˵�ÿҳ����Ŀ��\n��Χ5~��\nĬ��10",
                (void*)&Pack.MenuLinePerPage,
            {
                (const void*)&IBG_SettingPack::____MenuLinePerPage_Def,
                (const void*)&IBG_SettingPack::____MenuLinePerPage_Min,(const void*)&IBG_SettingPack::____MenuLinePerPage_Max,
            }
        },
        {
            IBB_SettingType::IntB,
                u8"�϶�����",u8"�����϶��ͱ�Ե���������ʻ�׼\n��Χ1~6\nĬ��3",
                (void*)&Pack.ScrollRateLevel,
            {
                (const void*)&IBG_SettingPack::____ScrollRate_Min,//Value.Min
                (const void*)&IBG_SettingPack::____ScrollRate_Max,//Value.Max
                (const void*)u8"%d ��",//Value.Format
            }
        },
        {
            IBB_SettingType::Bool,
                u8"��ɫģʽ",u8"����ȫ������Ϊ��ɫ���ǰ�ɫ��\nĬ��Ϊ��ɫģʽ",
                (void*)&Pack.DarkMode,
            {
                (const void*)+[]() { IBR_Color::StyleLight(); },//Action.SwitchToFalse
                (const void*)+[]() { IBR_Color::StyleDark(); }//Action.SwitchToTrue
            }
        },
        {
            IBB_SettingType::Bool,
                u8"��������ļ���",u8"�����������Ƿ��Զ����ļ���\nĬ�ϴ�",
                (void*)&Pack.OpenFolderOnOutput,
            {
                (const void*)nullptr,
                (const void*)nullptr,
            }
        },
        {
            IBB_SettingType::Bool,
                u8"������Զ�����",u8"����������Ƿ��Զ�����\nĬ�ϲ��Զ�����",
                (void*)&Pack.OutputOnSave,
            {
                (const void*)nullptr,
                (const void*)nullptr,
            }
        }
    };
}
std::vector<int> RW_ReadOrder = { 0,1,2,4,3,5,6 };
std::vector<int> RW_WriteOrder = { 0,1,2,4,3,5,6 };


void IBB_SettingTypeList::PackSetDefault()
{
    Pack.SetDefault();
    LastOutputDir.clear();
}

const IBG_SettingPack& IBG_GetSetting()
{
    return GlobalSettingPack;
}

void IBB_SetGlobalSetting(const IBG_SettingPack& Pack)
{
    GlobalSettingPack = Pack;
}

/*
�汾��չ��

ReadFileHeader IBB_RSetting2
{
    "IBB_Setting_F_201" ,
     [](const ExtFileClass& File,int Version,int Length)-> bool
    {
        if (Version > 201)
        {
            return ReadSettingFileGen(File, 1);
        }
        return false;
    }
};

WriteFileHeader IBB_WSetting2
{
    "IBB_Setting_F_201" ,
     [](const ExtFileClass& File)-> bool
    {
        return ReadSettingFileGen(File, 1);
    }
};

void IBB_SettingRegisterRW(SaveFile& Save)
{
    Save.ReadBlockProcess[IBB_RSetting.UsingID] = IBB_RSetting;
    Save.WriteBlockProcess.push_back(IBB_WSetting);
    Save.ReadBlockProcess[IBB_RSetting2.UsingID] = IBB_RSetting;
    Save.WriteBlockProcess.push_back(IBB_WSetting2);
}

*/
