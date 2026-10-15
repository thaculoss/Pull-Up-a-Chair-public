Scriptname PullUpAChairServiceControl extends Quest

Function ClearServiceAlias(Int ticket)
    ; Saved or delayed VM calls cannot clear a newer invitation.
    if !PullUpAChairService.IsReleaseCurrent(ticket)
        return
    endif
    ReferenceAlias serviceActor = GetAlias(0) as ReferenceAlias
    Actor employee = serviceActor.GetActorReference()
    serviceActor.Clear()
    if employee
        employee.EvaluatePackage()
    endif
    PullUpAChairService.ReleaseComplete(ticket)
EndFunction
