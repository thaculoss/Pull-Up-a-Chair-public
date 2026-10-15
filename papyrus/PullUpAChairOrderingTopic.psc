Scriptname PullUpAChairOrderingTopic extends TopicInfo Hidden
Function Fragment_0(ObjectReference akSpeakerRef)
    (GetOwningQuest() as PullUpAChairOrderingQuest).Order(akSpeakerRef as Actor)
EndFunction
