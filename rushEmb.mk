##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=rushEmb
ConfigurationName      :=Debug
WorkspacePath          :=C:/Work/NYCE5_testing/code_lite/nyce_socket
ProjectPath            :=C:/Work/NYCE5_testing/Eclipse/rushEmb
IntermediateDirectory  :=./Debug
OutDir                 := $(IntermediateDirectory)
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=rushdi
Date                   :=24/10/2017
CodeLitePath           :="C:/Program Files/CodeLite"
LinkerName             :=C:/devuser/toolchain/toolchain-rexroth-50.4.0.1/arm-rexroth-linux-gnueabihf/bin/arm-rexroth-linux-gnueabihf-g++.exe
SharedObjectLinkerName :=C:/devuser/toolchain/toolchain-rexroth-50.4.0.1/arm-rexroth-linux-gnueabihf/bin/arm-rexroth-linux-gnueabihf-g++.exe -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.i
DebugSwitch            :=-g 
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
OutputFile             :=$(IntermediateDirectory)/$(ProjectName)
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E
ObjectsFileList        :="rushEmb.txt"
PCHCompileFlags        :=
MakeDirCommand         :=makedir
RcCmpOptions           := 
RcCompilerName         :=windres
LinkOptions            :=  
IncludePath            :=  $(IncludeSwitch). $(IncludeSwitch). 
IncludePCH             := 
RcIncludePath          := 
Libs                   := $(LibrarySwitch)nyce $(LibrarySwitch)sac $(LibrarySwitch)sys $(LibrarySwitch)nhi $(LibrarySwitch)rt $(LibrarySwitch)pthread 
ArLibs                 :=  "nyce" "sac" "sys" "nhi" "rt" "pthread" 
LibPath                := $(LibraryPathSwitch). 

##
## Common variables
## AR, CXX, CC, AS, CXXFLAGS and CFLAGS can be overriden using an environment variables
##
AR       := C:/devuser/toolchain/toolchain-rexroth-50.4.0.1/arm-rexroth-linux-gnueabihf/bin/arm-rexroth-linux-gnueabihf-ar.exe rcu
CXX      := C:/devuser/toolchain/toolchain-rexroth-50.4.0.1/arm-rexroth-linux-gnueabihf/bin/arm-rexroth-linux-gnueabihf-g++.exe
CC       := C:/devuser/toolchain/toolchain-rexroth-50.4.0.1/arm-rexroth-linux-gnueabihf/bin/arm-rexroth-linux-gnueabihf-gcc.exe
CXXFLAGS :=  -g -O0 -Wall $(Preprocessors)
CFLAGS   :=  -g -O0 -Wall $(Preprocessors)
ASFLAGS  := 
AS       := C:/devuser/toolchain/toolchain-rexroth-50.4.0.1/arm-rexroth-linux-gnueabihf/bin/arm-rexroth-linux-gnueabihf-as.exe


##
## User defined environment variables
##
CodeLiteDir:=C:\Program Files\CodeLite
Objects0=$(IntermediateDirectory)/src_rushEmb.c$(ObjectSuffix) $(IntermediateDirectory)/src_select.c$(ObjectSuffix) 



Objects=$(Objects0) 

##
## Main Build Targets 
##
.PHONY: all clean PreBuild PrePreBuild PostBuild MakeIntermediateDirs
all: $(OutputFile)

$(OutputFile): $(IntermediateDirectory)/.d $(Objects) 
	@$(MakeDirCommand) $(@D)
	@echo "" > $(IntermediateDirectory)/.d
	@echo $(Objects0)  > $(ObjectsFileList)
	$(LinkerName) $(OutputSwitch)$(OutputFile) @$(ObjectsFileList) $(LibPath) $(Libs) $(LinkOptions)

MakeIntermediateDirs:
	@$(MakeDirCommand) "./Debug"


$(IntermediateDirectory)/.d:
	@$(MakeDirCommand) "./Debug"

PreBuild:


##
## Objects
##
$(IntermediateDirectory)/src_rushEmb.c$(ObjectSuffix): src/rushEmb.c $(IntermediateDirectory)/src_rushEmb.c$(DependSuffix)
	$(CC) $(SourceSwitch) "C:/Work/NYCE5_testing/Eclipse/rushEmb/src/rushEmb.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_rushEmb.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_rushEmb.c$(DependSuffix): src/rushEmb.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_rushEmb.c$(ObjectSuffix) -MF$(IntermediateDirectory)/src_rushEmb.c$(DependSuffix) -MM src/rushEmb.c

$(IntermediateDirectory)/src_rushEmb.c$(PreprocessSuffix): src/rushEmb.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_rushEmb.c$(PreprocessSuffix) src/rushEmb.c

$(IntermediateDirectory)/src_select.c$(ObjectSuffix): src/select.c $(IntermediateDirectory)/src_select.c$(DependSuffix)
	$(CC) $(SourceSwitch) "C:/Work/NYCE5_testing/Eclipse/rushEmb/src/select.c" $(CFLAGS) $(ObjectSwitch)$(IntermediateDirectory)/src_select.c$(ObjectSuffix) $(IncludePath)
$(IntermediateDirectory)/src_select.c$(DependSuffix): src/select.c
	@$(CC) $(CFLAGS) $(IncludePath) -MG -MP -MT$(IntermediateDirectory)/src_select.c$(ObjectSuffix) -MF$(IntermediateDirectory)/src_select.c$(DependSuffix) -MM src/select.c

$(IntermediateDirectory)/src_select.c$(PreprocessSuffix): src/select.c
	$(CC) $(CFLAGS) $(IncludePath) $(PreprocessOnlySwitch) $(OutputSwitch) $(IntermediateDirectory)/src_select.c$(PreprocessSuffix) src/select.c


-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) -r ./Debug/


