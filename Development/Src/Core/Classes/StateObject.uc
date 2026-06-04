class StateObject extends Object
	abstract
	native;

var private native transient Pointer StateFrame;

// Export UStateObject::execGotoState(FFrame&, void* const)
native(113) noexport final function GotoState(optional name NewState, optional name Label, optional bool bForceEvents, optional bool bKeepStack);

// Export UStateObject::execIsInState(FFrame&, void* const)
native(281) noexport final function bool IsInState(name TestState, optional bool bTestStateStack);

// Export UStateObject::execIsChildState(FFrame&, void* const)
native noexport final function bool IsChildState(name TestState, name TestParentState);

// Export UStateObject::execGetStateName(FFrame&, void* const)
native(284) noexport final function name GetStateName();

// Export UStateObject::execPushState(FFrame&, void* const)
native noexport final function PushState(name NewState, optional name NewLabel);

// Export UStateObject::execPopState(FFrame&, void* const)
native noexport final function PopState(optional bool bPopAll);

// Export UStateObject::execDumpStateStack(FFrame&, void* const)
native noexport final function DumpStateStack();

event BeginState(name PreviousStateName)
{
	return;
}

event EndState(name NextStateName)
{
	return;
}

event PushedState()
{
	return;
}

event PoppedState()
{
	return;
}

event PausedState()
{
	return;
}

event ContinuedState()
{
	return;
}

// Export UStateObject::execEnable(FFrame&, void* const)
native(117) noexport final function Enable(name ProbeFunc);

// Export UStateObject::execDisable(FFrame&, void* const)
native(118) noexport final function Disable(name ProbeFunc);
