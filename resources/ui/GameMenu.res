// GModPort main menu: GMod's menu entries that work without Lua/Steam.
"GameMenu"
{
	"1"
	{
		"label" "Resume Game"
		"command" "ResumeGame"
		"OnlyInGame" "1"
	}
	"2"
	{
		"label" " "
		"command" "spacer"
		"OnlyInGame" "1"
	}
	"3"
	{
		"label" "Start New Game: gm_construct"
		"command" "engine map gm_construct"
	}
	"4"
	{
		"label" "Start New Game: gm_flatgrass"
		"command" "engine map gm_flatgrass"
	}
	"5"
	{
		"label" " "
		"command" "spacer"
	}
	"6"
	{
		"label" "Options"
		"command" "OpenOptionsDialog"
	}
	"7"
	{
		"label" " "
		"command" "spacer"
	}
	"8"
	{
		"label" "Disconnect"
		"command" "engine disconnect"
		"OnlyInGame" "1"
	}
	"9"
	{
		"label" "Quit"
		"command" "Quit"
	}
}
