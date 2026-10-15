Scriptname PullUpAChairOrdering Hidden Native

Int Function Begin(Actor seller, FormList supportedFoods, Int[] courses, Int price) Global Native
Int Function GetStatus(Int ticket) Global Native
Form[] Function Selection(Int ticket) Global Native
Bool Function Commit(Int ticket) Global Native
Function Cancel(Int ticket) Global Native
Int Function FoodIndex(FormList supportedFoods, Form food) Global Native
Float Function ProfileValue(Int index, Int field) Global Native
Float[] Function ProfileBounds(Int index) Global Native

Float[] Function TableSpec(Int index) Global Native
Float Function TableMinSeatHeight(Int index) Global Native
ObjectReference[] Function FindTables(ObjectReference center, Float radius) Global Native
Int Function TableIndex(Form base) Global Native
Int Function TableProfileAt(ObjectReference center, Int slot) Global Native
Bool Function IsCounter(Int index) Global Native
Bool Function IsRoundTable(Int index) Global Native
Float[] Function PropBounds(Int index) Global Native
Bool Function NeedsPlate(Form food) Global Native
Float Function MainServingScale(Form mainDish) Global Native
Float Function ServingScale(Form food, Int course) Global Native
Float[] Function SelectionScales(Int ticket) Global Native
Bool Function TrackServing(Int ticket, ObjectReference item, ObjectReference seat, ObjectReference table, Location inn, Int kind) Global Native
Bool Function IsDiagnosticEnabled() Global Native
String Function Text(String key) Global Native
Function ClearTable(Actor server) Global Native
Function RestoreTable(Actor server) Global Native
Bool Function CanWalkForService(Actor server) Global Native
