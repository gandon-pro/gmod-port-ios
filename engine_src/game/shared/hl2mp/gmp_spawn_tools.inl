#include "vgui_controls/TextEntry.h"
#include "vgui_controls/Button.h"
#include <SDL.h>
#include "vgui_controls/Label.h"
#include "vgui_controls/Slider.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/PanelListPanel.h"
#include "gmp_ui_model.h"
#include "gmp_qskin.inl"

// Source text entries need an explicit SDL keyboard request on iOS.
class CGMPQTextEntry : public vgui::TextEntry {
 DECLARE_CLASS_SIMPLE(CGMPQTextEntry,vgui::TextEntry);
public:
 CGMPQTextEntry(vgui::Panel *parent,const char *name):BaseClass(parent,name),m_Hint(NULL){m_Hint=new vgui::Label(this,"Hint","");m_Hint->SetFgColor(Color(128,128,128,255));m_Hint->SetMouseInputEnabled(false);}
 void SetHint(const char *text){m_Hint->SetText(text);}
 void SetFont(vgui::HFont font) override{BaseClass::SetFont(font);if(m_Hint)m_Hint->SetFont(font);}
 void PerformLayout() override{BaseClass::PerformLayout();m_Hint->SetBounds(5,0,MAX(1,GetWide()-10),GetTall());}
 void OnThink() override{BaseClass::OnThink();m_Hint->SetVisible(GetTextLength()==0&&!HasFocus());}

 void RootKeyboard(bool enabled){for(vgui::Panel *p=GetParent();p;p=p->GetParent())if(!Q_strcmp(p->GetName(),"GMPSpawnMenu")){p->SetKeyBoardInputEnabled(enabled);return;}}
 void OnMousePressed(vgui::MouseCode code) override{if(code==MOUSE_LEFT)RootKeyboard(true);BaseClass::OnMousePressed(code);if(code==MOUSE_LEFT)SDL_StartTextInput();}
 void OnSetFocus() override{BaseClass::OnSetFocus();RootKeyboard(true);SDL_StartTextInput();}
 void OnKillFocus() override{BaseClass::OnKillFocus();SDL_StopTextInput();RootKeyboard(false);}

private:
 vgui::Label *m_Hint;
};

class CGMPToolOption : public vgui::EditablePanel {
 DECLARE_CLASS_SIMPLE(CGMPToolOption,vgui::EditablePanel);
public:
 CGMPToolOption(vgui::Panel *parent,const char *mode,const GMPToolVar &spec):BaseClass(parent,"QOption"),m_Spec(spec),m_Busy(true),m_Text(NULL),m_Slider(NULL),m_Check(NULL),m_Choice(NULL){
  Q_snprintf(m_Name,sizeof(m_Name),"%s_%s",mode,spec.name);
  m_Label=new vgui::Label(this,"Label",spec.label);m_Label->SetFgColor(Color(20,20,20,255));m_Label->SetMouseInputEnabled(false);
  if(spec.kind=='b'){m_Check=new vgui::CheckButton(this,"Check",spec.label);m_Check->SetFgColor(Color(20,20,20,255));m_Check->AddActionSignalTarget(this);m_Label->SetVisible(false);}
  else if(spec.kind=='s'){
   m_Choice=new vgui::ComboBox(this,"Choice",10,false);char choices[4096];Q_strncpy(choices,spec.choices?spec.choices:"",sizeof(choices));char *save=NULL;
   for(char *part=strtok_r(choices,"|",&save);part;part=strtok_r(NULL,"|",&save)){char *caption=strchr(part,'=');if(caption)*caption++=0;else caption=part;KeyValues kv("choice");kv.SetString("value",part);m_Choice->AddItem(caption,&kv);}
   m_Choice->AddActionSignalTarget(this);
  }else{
   m_Text=new CGMPQTextEntry(this,"Value");m_Text->AddActionSignalTarget(this);
   if(spec.kind=='n'){m_Slider=new vgui::Slider(this,"Slider");m_Slider->SetRange(0,1000);m_Slider->AddActionSignalTarget(this);}
  }
  m_Last[0]=0;Sync(true);m_Busy=false;SetTall(MAX(44,ScreenHeight()/14));
 }
 void PerformLayout() override{
  BaseClass::PerformLayout();int w,h;GetSize(w,h);int line=h/2;
  m_Label->SetBounds(4,0,w-8,line);
  if(m_Check)m_Check->SetBounds(4,0,w-8,h);
  if(m_Choice)m_Choice->SetBounds(4,line,w-8,line-2);
  if(m_Text)m_Text->SetBounds(m_Slider?w*2/3:4,line,m_Slider?w/3-4:w-8,line-2);
  if(m_Slider)m_Slider->SetBounds(2,line,w*2/3-4,line-2);
 }
 void OnThink() override{BaseClass::OnThink();Sync(false);}
private:
 void Sync(bool force){
  if((m_Text&&m_Text->HasFocus())||(m_Slider&&m_Slider->IsDragged()))return;
  ConVarRef cv(m_Name);const char *current=cv.IsValid()?cv.GetString():m_Spec.def;
  if(!force&&!Q_strcmp(current,m_Last))return;Q_strncpy(m_Last,current,sizeof(m_Last));m_Busy=true;
  if(m_Check)m_Check->SetSelected(atoi(current)!=0);
  if(m_Text)m_Text->SetText(current);
  if(m_Slider)m_Slider->SetValue(gmpui::SliderPosition(atof(current),m_Spec.min,m_Spec.max),false);
  if(m_Choice){for(int i=0;i<m_Choice->GetItemCount();++i){int id=m_Choice->GetItemIDFromRow(i);KeyValues *data=m_Choice->GetItemUserData(id);if(data&&!Q_strcmp(current,data->GetString("value"))){m_Choice->SilentActivateItem(id);break;}}}
  m_Busy=false;
 }
 MESSAGE_FUNC(OnText,"TextChanged"){
  if(m_Busy)return;ConVarRef cv(m_Name);if(!cv.IsValid())return;
  if(m_Choice){KeyValues *data=m_Choice->GetActiveItemUserData();if(data)cv.SetValue(data->GetString("value"));return;}
  if(!m_Text)return;char text[256];m_Text->GetText(text,sizeof(text));
  if(m_Spec.kind=='n'||m_Spec.kind=='k'){double value;if(!gmpui::ParseNumber(text,m_Spec.kind=='k'?0:m_Spec.min,m_Spec.kind=='k'?BUTTON_CODE_LAST-1:m_Spec.max,value))return;if(m_Spec.kind=='k')value=floor(value);cv.SetValue(static_cast<float>(value));if(m_Slider)m_Slider->SetValue(gmpui::SliderPosition(value,m_Spec.min,m_Spec.max),false);}
  else cv.SetValue(text);
 }
 MESSAGE_FUNC(OnSlide,"SliderMoved"){
  if(m_Busy||!m_Slider)return;float n=gmpui::SliderValue(m_Slider->GetValue(),m_Spec.min,m_Spec.max);ConVarRef cv(m_Name);if(cv.IsValid())cv.SetValue(n);
  m_Busy=true;char text[64];Q_snprintf(text,sizeof(text),"%.4g",n);m_Text->SetText(text);m_Busy=false;
 }
 MESSAGE_FUNC(OnCheck,"CheckButtonChecked"){if(!m_Busy&&m_Check){ConVarRef cv(m_Name);if(cv.IsValid())cv.SetValue(m_Check->IsSelected()?1:0);}}
 GMPToolVar m_Spec;char m_Name[96],m_Last[256];bool m_Busy;vgui::Label *m_Label;vgui::TextEntry *m_Text;vgui::Slider *m_Slider;vgui::CheckButton *m_Check;vgui::ComboBox *m_Choice;
};

class CGMPToolList : public CGMPDragPanel {
 DECLARE_CLASS_SIMPLE(CGMPToolList,CGMPDragPanel);
 struct Row{int tool,category;};
public:
 CGMPToolList(vgui::Panel *parent):BaseClass(parent,"QTools"),m_Font(0),m_Row(28),m_Tool(-1){m_Filter[0]=0;for(int i=0;i<3;++i)m_Collapsed[i]=false;Build();}
 void SetFilter(const char *filter){Q_strncpy(m_Filter,filter,sizeof(m_Filter));Build();}
 void SetFont(vgui::HFont font){m_Font=font;m_Row=MAX(24,vgui::surface()->GetFontTall(font)*2);Build();}
 void SetTool(int tool){m_Tool=tool;Repaint();}
 void Paint() override{
  if(!s_GMPQSkin.Draw("Tree",0,0,GetWide(),GetTall())){vgui::surface()->DrawSetColor(255,255,255,255);vgui::surface()->DrawFilledRect(0,0,GetWide(),GetTall());}
  for(int i=0;i<m_Rows.Count();++i){int y=i*m_Row-m_nScroll;if(y+m_Row<0||y>=GetTall())continue;const Row &row=m_Rows[i];
   if(row.tool<0){s_GMPQSkin.Draw("CategoryList.Header",0,y,GetWide(),m_Row);Text(m_Collapsed[row.category]&&!*m_Filter?"+":"-",5,y,Color(30,30,30,255));Text(s_Categories[row.category],20,y,Color(30,30,30,255));}
   else{if(row.tool==m_Tool){vgui::surface()->DrawSetColor(100,170,220,255);vgui::surface()->DrawFilledRect(2,y,GetWide()-2,y+m_Row);}Text(s_GMPToolDefs[row.tool].name,8,y,Color(20,20,20,255));}
  }
 }
protected:
 void OnTap(int x,int y) override{int index=y/m_Row;if(!m_Rows.IsValidIndex(index))return;const Row &row=m_Rows[index];if(row.tool<0){m_Collapsed[row.category]=!m_Collapsed[row.category];Build();}else PostActionSignal(new KeyValues("QToolSelected","tool",row.tool));}
private:
 void Build(){m_Rows.RemoveAll();for(int c=0;c<3;++c){bool categoryMatch=gmpui::Matches(m_Filter,s_Categories[c],"");int count=0;for(int i=0;i<ARRAYSIZE(s_GMPToolDefs);++i)if(!Q_strcmp(s_GMPToolDefs[i].category,s_Categories[c])&&(categoryMatch||gmpui::Matches(m_Filter,s_GMPToolDefs[i].name,s_GMPToolDefs[i].mode)))++count;if(!count)continue;Row header={-1,c};m_Rows.AddToTail(header);if(m_Collapsed[c]&&!*m_Filter)continue;for(int i=0;i<ARRAYSIZE(s_GMPToolDefs);++i)if(!Q_strcmp(s_GMPToolDefs[i].category,s_Categories[c])&&(categoryMatch||gmpui::Matches(m_Filter,s_GMPToolDefs[i].name,s_GMPToolDefs[i].mode))){Row row={i,c};m_Rows.AddToTail(row);}}
  m_nContentTall=m_Rows.Count()*m_Row;SetScroll(m_nScroll);InvalidateLayout();Repaint();}
 void Text(const char *text,int x,int y,Color color){wchar_t wide[256];g_pVGuiLocalize->ConvertANSIToUnicode(text,wide,sizeof(wide));vgui::surface()->DrawSetTextFont(m_Font);vgui::surface()->DrawSetTextColor(color);vgui::surface()->DrawSetTextPos(x,y+(m_Row-vgui::surface()->GetFontTall(m_Font))/2);vgui::surface()->DrawPrintText(wide,wcslen(wide));}
 static const char *const s_Categories[3];CUtlVector<Row> m_Rows;vgui::HFont m_Font;int m_Row,m_Tool;char m_Filter[128];bool m_Collapsed[3];
};
const char *const CGMPToolList::s_Categories[3]={"Constraints","Construction","Render"};

class CGMPSpawnTools : public vgui::EditablePanel {
 DECLARE_CLASS_SIMPLE(CGMPSpawnTools,vgui::EditablePanel);
public:
 CGMPSpawnTools(vgui::Panel *parent):BaseClass(parent,"QToolMenu"),m_Tool(-1),m_Utilities(false){
  SetBgColor(Color(235,235,235,255));m_Search=new CGMPQTextEntry(this,"ToolSearch");m_Search->AddActionSignalTarget(this);static_cast<CGMPQTextEntry *>(m_Search)->SetHint("Quick filter...");
  m_List=new CGMPToolList(this);m_List->AddActionSignalTarget(this);m_Title=new vgui::Label(this,"Title","Tools");m_Title->SetFgColor(Color(20,20,20,255));
  m_Help=new vgui::Label(this,"Help","");m_Help->SetWrap(true);m_Help->SetFgColor(Color(50,50,50,255));m_Help->SetMouseInputEnabled(false);
  m_Options=new vgui::PanelListPanel(this,"ControlPanel");m_Options->SetFirstColumnWidth(0);m_Options->SetBgColor(Color(235,235,235,255));
 }
 void SetFont(vgui::HFont font){m_List->SetFont(font);m_Title->SetFont(font);m_Help->SetFont(font);m_Search->SetFont(font);}
 void PerformLayout() override{
  BaseClass::PerformLayout();int w=GetWide(),h=GetTall(),row=MAX(24,ScreenHeight()/30),list=MAX(110,w*35/100);
  m_Search->SetBounds(2,2,list-4,row);m_List->SetBounds(2,row+5,list-4,h-row-7);
  m_Title->SetBounds(list+8,2,w-list-12,row);m_Help->SetBounds(list+8,row+3,w-list-12,row*3);
  m_Options->SetBounds(list+6,row*4+8,w-list-8,MAX(1,h-row*4-10));
  m_Search->SetVisible(!m_Utilities);m_List->SetVisible(!m_Utilities);
  if(m_Utilities){m_Title->SetBounds(8,2,w-16,row);m_Help->SetBounds(8,row+3,w-16,row*2);m_Options->SetBounds(4,row*3+8,w-8,h-row*3-12);}
 }
 void OnThink() override{BaseClass::OnThink();ConVarRef mode("gmod_toolmode");int tool=mode.IsValid()?GMP_ToolIndex(mode.GetString()):-1;if(!m_Utilities&&tool!=m_Tool)ShowTool(tool);}
 void ShowUtilities(bool enabled){m_Utilities=enabled;m_Tool=-1;m_Options->DeleteAllItems();if(enabled){m_Title->SetText("Utilities");m_Help->SetText("Actions apply to objects you spawned.");AddAction("Undo","gmod_undo");AddAction("Clean up","gmod_cleanup");AddAction("Save contraption","gm_dupe_save");AddAction("Save world","gm_save");}else{ConVarRef mode("gmod_toolmode");ShowTool(mode.IsValid()?GMP_ToolIndex(mode.GetString()):-1);}InvalidateLayout();}
private:
 void AddAction(const char *name,const char *command){vgui::Button *button=new vgui::Button(m_Options,"Action",name,this,command);button->SetTall(MAX(28,ScreenHeight()/20));m_Options->AddItem(NULL,button);}
 void OnCommand(const char *command) override{if(!Q_strcmp(command,"q_reset_tool")){if(m_Tool>=0&&m_Tool<ARRAYSIZE(s_GMPToolDefs)){const GMPToolDef &def=s_GMPToolDefs[m_Tool];for(int i=0;i<def.numVars;++i){char name[96];Q_snprintf(name,sizeof(name),"%s_%s",def.mode,def.vars[i].name);ConVarRef cv(name);if(cv.IsValid())cv.SetValue(def.vars[i].def);}}return;}if(!Q_strcmp(command,"gmod_undo")||!Q_strcmp(command,"gmod_cleanup")||!Q_strcmp(command,"gm_dupe_save")||!Q_strcmp(command,"gm_save")){engine->ClientCmd_Unrestricted(command);return;}BaseClass::OnCommand(command);}
 MESSAGE_FUNC(OnSearch,"TextChanged"){char filter[128];m_Search->GetText(filter,sizeof(filter));m_List->SetFilter(filter);}
 MESSAGE_FUNC_PARAMS(OnTool,"QToolSelected",data){int tool=data->GetInt("tool",-1);if(tool<0||tool>=ARRAYSIZE(s_GMPToolDefs))return;ConVarRef mode("gmod_toolmode");if(mode.IsValid())mode.SetValue(s_GMPToolDefs[tool].mode);engine->ClientCmd_Unrestricted("use gmod_tool\n");ShowTool(tool);}
 void ShowTool(int tool){m_Tool=tool;m_Options->DeleteAllItems();m_List->SetTool(tool);if(tool<0||tool>=ARRAYSIZE(s_GMPToolDefs)){m_Title->SetText("No tool selected");m_Help->SetText("");return;}
  const GMPToolDef &def=s_GMPToolDefs[tool];m_Title->SetText(def.name);m_Help->SetText(def.left?def.left:"");
  for(int i=0;i<def.numVars;++i)m_Options->AddItem(NULL,new CGMPToolOption(m_Options,def.mode,def.vars[i]));
  if(def.numVars)AddAction("Reset defaults","q_reset_tool");
 }
 int m_Tool;bool m_Utilities;CGMPToolList *m_List;vgui::TextEntry *m_Search;vgui::Label *m_Title,*m_Help;vgui::PanelListPanel *m_Options;
};
