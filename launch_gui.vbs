' Starts launch_gui.bat with its console hidden (no window flash at all).
Set fso = CreateObject("Scripting.FileSystemObject")
Set shell = CreateObject("WScript.Shell")
shell.CurrentDirectory = fso.GetParentFolderName(WScript.ScriptFullName)
shell.Run """" & shell.CurrentDirectory & "\launch_gui.bat""", 0, False
