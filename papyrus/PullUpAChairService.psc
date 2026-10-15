Scriptname PullUpAChairService Hidden Native

; All requests are marshalled to the game thread by this module.
Function CallWaiter() Global Native
Function CancelService() Global Native
Function RefreshSupply() Global Native
Bool Function IsReleaseCurrent(Int ticket) Global Native
Function ReleaseComplete(Int ticket) Global Native
