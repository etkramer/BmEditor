echo off
rem Usage: VocabBld <word list.txt> <dictionary.pdc> <nnet.pni> <output.xvocab>
rem   Utility to create a speech recognition file.
rem     <Word list.txt> has one line per word to recognize,
rem                     with each line containing:
rem                     <ID# to return><tab><word or phrase>
rem                     (lines beginning with non-numeric characters are ignored)
rem     <Dictionary.pdc> is a dictionary file provided by Fonix corporation
rem     <AsrNNet.pni>    is a 'packed net file' from Fonix corporation
rem     <Output.xvocab>  is the name of the output file to create.

rem Usage: Buildnnx <OutputFnxVoice.vnn> <OutputFnxVoiceUser.usr> <inputNNet.pni>
rem  <vocab1.xvocab> [vocab2.xvocab...]
rem         [-s MaxSimultaneousVocabs] [-p MaxTotalSearchPaths] [-n MaxTotalNodes]

md PS2

rem ==== "Tasty" (70-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Tasty.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\usgp11N3054.pni .\PS2\Tasty.xvocab -n 10 -p 25

..\..\..\tools\Win32\bin\BuildVoice .\PS2\Tasty.vnn .\PS2\Tasty.usr ..\..\..\Languages\USEnglish\usgp11N3054.pni .\PS2\Tasty.xvocab 


pause

rem ==== "Monsters" (70-word, single-user) demo ====
..\..\..\tools\Win32\bin\VocabBld Monsters.txt ..\..\..\Languages\USEnglish\USEnglish.pdc ..\..\..\Languages\USEnglish\usgp11N3054.pni .\PS2\Monsters.xvocab -n 5

..\..\..\tools\Win32\bin\BuildVoice .\PS2\Monsters.vnn .\PS2\Monsters.usr ..\..\..\Languages\USEnglish\usgp11N3054.pni .\PS2\Monsters.xvocab 


pause

