Scriptname PullUpAChairOrderingQuest extends Quest
Actor Property PlayerRef Auto
MiscObject Property Gold001 Auto
GlobalVariable Property OrderPrice Auto

Function Order(Actor seller)
    LocalServe(seller, 0)
EndFunction

; A service call only requests ordinary engine pathing. The native core checks
; distance, timeout and seated context, then starts normal conversation on arrival.
; This function does not create food, charge gold or open the ordering menu.
bool Function WalkForService(Actor seller)
    ; A queued VM helper must recheck live ownership when it actually runs.
    if !PullUpAChairOrdering.CanWalkForService(seller)
        return false
    endif
    if !seller || seller.IsDead() || seller.IsInCombat() || seller.GetCurrentScene() || seller.IsInDialogueWithPlayer() || PlayerRef.GetSitState() != 3 || seller.GetParentCell() != PlayerRef.GetParentCell()
        return false
    endif
    return seller.PathToReference(PlayerRef, 0.0)
EndFunction

int LocalTicket = 0
ObjectReference LocalTable
bool LocalHasPlate = false
bool LocalBusy = false
ObjectReference[] LocalMeal
Cell LocalMealCell
float LocalX
float LocalY
float LocalZ
float LocalYaw
float LocalDrinkSide = 35.0
float LocalExtraSide = -42.0
float LocalDrinkForward = 14.0
float LocalExtraForward = 8.0
bool LocalDiagnostics = false
float LocalPlateScale
float LocalMainRise
float LocalMainWidth
float LocalMainDepth
float LocalMainFoodScale = 1.0
float[] LocalFoodScales

bool Function LocalServe(Actor seller, int price)
    int galleryTicket = 0
    if LocalBusy && PullUpAChairOrdering.GetStatus(LocalTicket) == 0
        LocalBusy = false
        LocalMeal = new ObjectReference[4]
    endif
    if LocalBusy
        Debug.Notification(PullUpAChairOrdering.Text("busy"))
        return false
    endif
    LocalBusy = true
    LocalDiagnostics = PullUpAChairOrdering.IsDiagnosticEnabled()
    float orderStarted = Utility.GetCurrentRealTime()
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Order started")
    endif
    UnregisterForUpdate()
    ObjectReference seat = PlayerRef.GetFurnitureReference()
    if !seller || PlayerRef.GetSitState() != 3 || !seat
        Debug.Notification(PullUpAChairOrdering.Text("sit_first"))
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    Cell room = PlayerRef.GetParentCell()
    if PlayerRef.IsInCombat() || seller.GetParentCell() != room
        Debug.Notification(PullUpAChairOrdering.Text("unavailable"))
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    Location inn = PullUpAChair(seller)
    if !inn
        Debug.Notification(PullUpAChairOrdering.Text("not_inn"))
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    FormList foodList = Game.GetFormFromFile(0x801, "Pull up a chair Ordering.esp") as FormList
    Form plate = Game.GetFormFromFile(0xB9BD0, "Skyrim.esm")
    if !foodList || foodList.GetSize() != 54
        Debug.Notification(PullUpAChairOrdering.Text("catalog_error"))
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    int[] galleryCourses = new int[54]
    int galleryIndex = 0
    while galleryIndex < 54
        galleryCourses[galleryIndex] = LocalFoodCourse(galleryIndex)
        galleryIndex += 1
    endwhile
    galleryTicket = PullUpAChairOrdering.Begin(seller, foodList, galleryCourses, price)
    LocalTicket = galleryTicket
    if galleryTicket == 0
        Debug.Notification(PullUpAChairOrdering.Text("menu_failed"))
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    while PullUpAChairOrdering.GetStatus(galleryTicket) == 1
        Utility.WaitMenuMode(0.1)
    endwhile
    if PullUpAChairOrdering.GetStatus(galleryTicket) != 2
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    Form[] rolled = PullUpAChairOrdering.Selection(galleryTicket)
    if !rolled || rolled.Length != 3
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    float[] selectedScales = PullUpAChairOrdering.SelectionScales(galleryTicket)
    if !selectedScales || selectedScales.Length != 3
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    Form[] dishes = new Form[4]
    LocalFoodScales = new float[3]
    int dishCount = 0
    int i = 0
    while i < 3
        if rolled[i]
            dishes[dishCount] = rolled[i]
            ; Preserve the selected course even when omitted courses compact the serving slots.
            LocalFoodScales[dishCount] = selectedScales[i]
            dishCount += 1
        endif
        i += 1
    endwhile
    if dishCount == 0
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    int mainIndex = PullUpAChairOrdering.FoodIndex(foodList, dishes[0])
    LocalHasPlate = rolled[0] && PullUpAChairOrdering.NeedsPlate(rolled[0])
    ; Snapshot this order's size once. Changing settings affects only future orders.
    LocalMainFoodScale = LocalFoodScales[0]
    LocalPlateScale = LocalPlateScaleFor(mainIndex) * LocalMainFoodScale
    LocalMainRise = 0.5
    LocalMainWidth = LocalFoodWidth(mainIndex) * LocalMainFoodScale
    LocalMainDepth = LocalFoodDepth(mainIndex) * LocalMainFoodScale
    if LocalHasPlate
        dishes[3] = plate
        LocalMainRise = LocalPlateSupportFor(mainIndex) * LocalPlateScale + 0.45
        LocalMainWidth = LocalMax(LocalMainWidth, 15.67728 * LocalPlateScale + 0.5)
        LocalMainDepth = LocalMax(LocalMainDepth, 15.67728 * LocalPlateScale + 0.5)
    endif
    LocalDrinkSide = 0.0
    LocalExtraSide = 0.0
    if !LocalFindTable(dishes, dishCount, foodList)
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        if LocalDiagnostics
            Debug.Trace("[InnOrdering 2.0] Search cancelled; unlocked, no charge")
        endif
        return false
    endif
    if PlayerRef.GetFurnitureReference() != seat || PlayerRef.GetSitState() != 3 || PlayerRef.GetParentCell() != room
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        Debug.Notification(PullUpAChairOrdering.Text("seat_changed"))
        return false
    endif
    ; Only drop old tracking handles. Never collect or delete previously paid food.
    LocalPackMeal()
    LocalMeal = new ObjectReference[4]
    LocalMealCell = room
    ActorBase owner = PlayerRef.GetActorBase()
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Spawning food and GlazedPlate01 main-course plate; scale=" + LocalPlateScale)
    endif
    i = 0
    bool failed = false
    while i < 4 && !failed
        if dishes[i]
            ; References are held for this transaction; do not force permanent persistence.
            ObjectReference item = PlayerRef.PlaceAtMe(dishes[i], 1, false, true)
            if !item
                failed = true
            else
                LocalMeal[i] = item
                item.BlockActivation(true)
                item.SetActorOwner(owner)
                if i == 3
                    item.SetScale(LocalPlateScale)
                elseif i < 3 && LocalSlotScale(i) < 1.0
                    item.SetScale(LocalSlotScale(i))
                endif
                LocalPositionServing(item, i, dishes[i], foodList)
                int kind = 0
                if i == 3
                    kind = 1
                elseif i > 0
                    kind = 2
                endif
                if !PullUpAChairOrdering.TrackServing(galleryTicket, item, seat, LocalTable, inn, kind)
                    failed = true
                endif
            endif
        endif
        i += 1
    endwhile
    if failed || PlayerRef.GetFurnitureReference() != seat || PlayerRef.GetSitState() != 3 || PlayerRef.GetParentCell() != room
        LocalDiscardUnpaid()
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        Debug.Notification(PullUpAChairOrdering.Text("cancelled"))
        return false
    endif
    i = 0
    while i < 4
        if LocalMeal[i]
            LocalMeal[i].EnableNoWait()
        endif
        i += 1
    endwhile
    failed = !LocalWaitModels(dishes, foodList, seat, room)
    if failed || PlayerRef.GetFurnitureReference() != seat || PlayerRef.GetSitState() != 3 || PlayerRef.GetParentCell() != room
        LocalDiscardUnpaid()
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        Debug.Notification(PullUpAChairOrdering.Text("models_failed"))
        return false
    endif
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Food and plate ready; charging once")
    endif
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Side dish ready; base=" + dishes[2] + " reference=" + LocalMeal[2])
    endif
    if !PullUpAChairOrdering.Commit(galleryTicket)
        LocalDiscardUnpaid()
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        Debug.Notification(PullUpAChairOrdering.Text("stock_changed"))
        return false
    endif
    while PullUpAChairOrdering.GetStatus(galleryTicket) == 4
        Utility.WaitMenuMode(0.1)
    endwhile
    if PullUpAChairOrdering.GetStatus(galleryTicket) != 3
        LocalDiscardUnpaid()
        PullUpAChairOrdering.Cancel(galleryTicket)
        LocalBusy = false
        return false
    endif
    i = 0
    while i < 4
        if LocalMeal[i] && i != 3
            LocalMeal[i].BlockActivation(false)
        endif
        i += 1
    endwhile
    LocalMeal = new ObjectReference[4]
    LocalMealCell = None
    LocalBusy = false
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Served; total elapsed=" + (Utility.GetCurrentRealTime() - orderStarted) + "; no packing or polling registered")
    endif
    Debug.Notification(PullUpAChairOrdering.Text("served"))
    return true
EndFunction

Function LocalPositionServing(ObjectReference item, int slot, Form food, FormList foodList)
    float sideways = LocalSlotSide(slot)
    float forward = LocalSlotForward(slot)
    float z = LocalZ + 0.5
    if slot == 3
        sideways = 0.0
        forward = 0.0
        z = LocalZ + 0.3 - 0.00000037 * LocalPlateScale
    else
        z -= LocalFoodBottom(PullUpAChairOrdering.FoodIndex(foodList, food)) * LocalSlotScale(slot)
        if slot == 0
            z = LocalZ + LocalMainRise - LocalFoodBottom(PullUpAChairOrdering.FoodIndex(foodList, food)) * LocalMainFoodScale
        endif
    endif
    item.SetAngle(0.0, 0.0, LocalYaw)
    item.SetPosition(LocalX + sideways * Math.Cos(LocalYaw) + forward * Math.Sin(LocalYaw), LocalY - sideways * Math.Sin(LocalYaw) + forward * Math.Cos(LocalYaw), z)
EndFunction

Location Function PullUpAChair(Actor seller)
    Keyword innKeyword = Game.GetFormFromFile(0x1CB87, "Skyrim.esm") as Keyword
    Location where = seller.GetCurrentLocation()
    int depth = 0
    while where && depth < 12
        if where.HasKeyword(innKeyword)
            return where
        endif
        where = where.GetParent()
        depth += 1
    endwhile
    return None
EndFunction

bool Function LocalWaitModels(Form[] dishes, FormList foodList, ObjectReference seat, Cell room)
    ; One deadline for the whole serving; each ready model is finalized once.
    bool[] prepared = new bool[4]
    int remaining = 4
    int polls = 0
    float started = Utility.GetCurrentRealTime()
    while remaining > 0
        if PlayerRef.GetFurnitureReference() != seat || PlayerRef.GetSitState() != 3 || PlayerRef.GetParentCell() != room
            return false
        endif
        int i = 0
        while i < 4
            if !prepared[i]
                if !dishes[i]
                    prepared[i] = true
                    remaining -= 1
                elseif !LocalMeal[i]
                    return false
                elseif LocalMeal[i].Is3DLoaded()
                    LocalMeal[i].SetMotionType(4, true)
                    if i < 3 && LocalSlotScale(i) < 1.0
                        LocalMeal[i].SetScale(LocalSlotScale(i))
                    elseif i == 3 && LocalMainFoodScale < 1.0
                        LocalMeal[i].SetScale(LocalPlateScale)
                    endif
                    LocalPositionServing(LocalMeal[i], i, dishes[i], foodList)
                    int kind = 0
                    if i == 3
                        kind = 1
                    elseif i > 0
                        kind = 2
                    endif
                    if !PullUpAChairOrdering.TrackServing(LocalTicket, LocalMeal[i], seat, LocalTable, PullUpAChair(PlayerRef), kind)
                        return false
                    endif
                    prepared[i] = true
                    remaining -= 1
                endif
            endif
            i += 1
        endwhile
        if remaining == 0
            return true
        endif
        float now = Utility.GetCurrentRealTime()
        if now < started || now - started >= 2.0 || polls >= 20
            return false
        endif
        Utility.Wait(0.1)
        polls += 1
    endwhile
    return true
EndFunction

float[] Function LocalLayoutOffsets(int layout, float[] widths, float[] depths)
    ; Three S/F pairs. Preserve layout 0 exactly; all alternatives retain a gap.
    float[] offsets = new float[6]
    offsets[2] = widths[0] + widths[1] + 3.0
    offsets[3] = 14.0
    offsets[4] = 0.0 - widths[0] - widths[2] - 3.0
    offsets[5] = 8.0
    if layout == 1
        offsets[2] = 0.0 - offsets[2]
        offsets[4] = 0.0 - offsets[4]
    elseif layout >= 2
        ; A compact row of drink and side dish behind (2) or before (3) the main.
        offsets[2] = widths[2] + 1.5
        offsets[4] = 0.0 - widths[1] - 1.5
        float rowForward = depths[0] + LocalMax(depths[1], depths[2]) + 3.0
        if layout == 3
            rowForward = 0.0 - rowForward
        endif
        offsets[3] = rowForward
        offsets[5] = rowForward
    endif
    return offsets
EndFunction

float Function LocalSlotSide(int slot)
    if slot == 1
        return LocalDrinkSide
    elseif slot == 2
        return LocalExtraSide
    elseif slot == 3
        return -32.0
    endif
    return 0.0
EndFunction

float Function LocalSlotForward(int slot)
    if slot == 1
        return LocalDrinkForward
    elseif slot == 2
        return LocalExtraForward
    elseif slot == 3
        return 33.0
    endif
    return 0.0
EndFunction

bool Function LocalPointOnTable(ObjectReference tableRef, float[] spec, float x, float y, float radius)
    float angle = tableRef.GetAngleZ()
    float scale = tableRef.GetScale()
    if scale < 0.1
        return false
    endif
    float dx = x - tableRef.GetPositionX()
    float dy = y - tableRef.GetPositionY()
    float tx = (dx * Math.Cos(angle) - dy * Math.Sin(angle)) / scale
    float ty = (dx * Math.Sin(angle) + dy * Math.Cos(angle)) / scale
    float margin = radius / scale
    return tx > spec[0] + margin && tx < spec[2] - margin && ty > spec[1] + margin && ty < spec[3] - margin
EndFunction

; v0.2: calibrated bar segments form one continuous rectangular serving surface.
; Merge only touching, coplanar rectangles with matching transverse edges.
; No bounding box is drawn across an L-shaped corner or a gap between tables.
float[] Function LocalBarSurface(ObjectReference anchor, float[] surface, ObjectReference[] tables, FormList tableList)
    float yaw = anchor.GetAngleZ()
    float scale = anchor.GetScale()
    float height = anchor.GetPositionZ() + surface[4] * scale
    int pass = 0
    bool changed = true
    while changed && pass < 8
        changed = false
        int j = 0
        while j < tables.Length
            ObjectReference other = tables[j]
            if other && other != anchor && other.GetParentCell() == anchor.GetParentCell() && !other.IsDisabled() && other.Is3DLoaded()
                int index = PullUpAChairOrdering.TableIndex(other.GetBaseObject())
                if index >= 25 && index < 31 && Math.Abs(other.GetAngleX()) < 1.0 && Math.Abs(other.GetAngleY()) < 1.0
                    float[] part = LocalTableSpec(index)
                    float otherScale = other.GetScale()
                    float angle = other.GetAngleZ() - yaw
                    float sn = Math.Sin(angle)
                    float cs = Math.Cos(angle)
                    if otherScale > 0.1 && Math.Abs(other.GetPositionZ() + part[4] * otherScale - height) < 0.5 && (Math.Abs(sn) < 0.001 || Math.Abs(cs) < 0.001)
                        float dx = other.GetPositionX() - anchor.GetPositionX()
                        float dy = other.GetPositionY() - anchor.GetPositionY()
                        float ratio = otherScale / scale
                        float cx = (dx * Math.Cos(yaw) - dy * Math.Sin(yaw)) / scale
                        float cy = (dx * Math.Sin(yaw) + dy * Math.Cos(yaw)) / scale
                        cx += ((part[0] + part[2]) * cs + (part[1] + part[3]) * sn) * ratio * 0.5
                        cy += (0.0 - (part[0] + part[2]) * sn + (part[1] + part[3]) * cs) * ratio * 0.5
                        float hx = ((part[2] - part[0]) * Math.Abs(cs) + (part[3] - part[1]) * Math.Abs(sn)) * ratio * 0.5
                        float hy = ((part[2] - part[0]) * Math.Abs(sn) + (part[3] - part[1]) * Math.Abs(cs)) * ratio * 0.5
                        float loX = cx - hx
                        float hiX = cx + hx
                        float loY = cy - hy
                        float hiY = cy + hy
                        ; Allow only tiny mesh/placement seams, not missing segments.
                        if Math.Abs(loX - surface[0]) < 0.05 && Math.Abs(hiX - surface[2]) < 0.05 && loY <= surface[3] + 0.02 && hiY >= surface[1] - 0.02
                            if loY < surface[1] - 0.02 || hiY > surface[3] + 0.02
                                surface[0] = LocalMax(surface[0], loX)
                                surface[2] = LocalMin(surface[2], hiX)
                                surface[1] = LocalMin(surface[1], loY)
                                surface[3] = LocalMax(surface[3], hiY)
                                changed = true
                            endif
                        elseif Math.Abs(loY - surface[1]) < 0.05 && Math.Abs(hiY - surface[3]) < 0.05 && loX <= surface[2] + 0.02 && hiX >= surface[0] - 0.02
                            if loX < surface[0] - 0.02 || hiX > surface[2] + 0.02
                                surface[1] = LocalMax(surface[1], loY)
                                surface[3] = LocalMin(surface[3], hiY)
                                surface[0] = LocalMin(surface[0], loX)
                                surface[2] = LocalMax(surface[2], hiX)
                                changed = true
                            endif
                        endif
                    endif
                endif
            endif
            j += 1
        endwhile
        pass += 1
    endwhile
    return surface
EndFunction

float Function LocalMin(float a, float b)
    if a < b
        return a
    endif
    return b
EndFunction

float Function LocalMax(float a, float b)
    if a > b
        return a
    endif
    return b
EndFunction

; v0.3: cache engine reference data once. All placement and strip merging below
; use numbers only; no reference queries are made inside the geometry loops.
bool Function LocalFindTable(Form[] dishes, int dishCount, FormList foodList)
    float started = Utility.GetCurrentRealTime()
    FormList tableList = Game.GetFormFromFile(0x800, "Pull up a chair Ordering.esp") as FormList
    if !tableList || tableList.GetSize() != 35
        Debug.Notification(PullUpAChairOrdering.Text("table_config"))
        return false
    endif
    ObjectReference chair = PlayerRef.GetFurnitureReference()
    Cell room = PlayerRef.GetParentCell()
    float px = PlayerRef.GetPositionX()
    float py = PlayerRef.GetPositionY()
    float chairZ = chair.GetPositionZ()
    float facing = PlayerRef.GetAngleZ()
    float pc = Math.Cos(facing)
    float ps = Math.Sin(facing)
    ; Native discovery includes model aliases and measures distance to the surface edge.
    ObjectReference[] tables = PullUpAChairOrdering.FindTables(PlayerRef, 300.0)
    if !tables || tables.Length == 0 || tables.Length > 64
        Debug.Notification(PullUpAChairOrdering.Text("no_table"))
        return false
    endif
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Snapshot seat=" + chair + " playerXY=" + px + "," + py + " facing=" + facing + " refs=" + tables.Length)
    endif
    float[] xs = new float[64]
    float[] ys = new float[64]
    float[] heights = new float[64]
    float[] angles = new float[64]
    float[] scales = new float[64]
    int[] profiles = new int[64]
    ObjectReference[] tableRefs = new ObjectReference[64]
    int n = 0
    int i = 0
    float bestDistance = 1000000.0
    float frame = facing
    while i < tables.Length
        ObjectReference ref = tables[i]
        if ref && !ref.IsDisabled() && ref.GetParentCell() == room && ref.Is3DLoaded()
            float dx = ref.GetPositionX() - px
            float dy = ref.GetPositionY() - py
            float forward = dx * ps + dy * pc
            float sideways = dx * pc - dy * ps
            ; FindTables already filters by distance to the actual edge, including long-table ends.
                ; A composite interior can supply several distinct tabletop regions.
                int index = PullUpAChairOrdering.TableProfileAt(PlayerRef, i)
                float[] spec = LocalTableSpec(index)
                float scale = ref.GetScale()
                float height = ref.GetPositionZ() + spec[4] * scale
                float delta = height - chairZ
                float minimumSeatHeight = PullUpAChairOrdering.TableMinSeatHeight(index)
                if index >= 0 && scale > 0.1 && delta > minimumSeatHeight && delta < 105.0 && Math.Abs(ref.GetAngleX()) < 1.0 && Math.Abs(ref.GetAngleY()) < 1.0
                    float angle = ref.GetAngleZ()
                    xs[n] = dx
                    ys[n] = dy
                    heights[n] = height
                    angles[n] = angle
                    scales[n] = scale
                    profiles[n] = index
                    tableRefs[n] = ref
                    float distance = dx * dx + dy * dy
                    if minimumSeatHeight < 35.0
                        ; Explicitly registered low tables may sit beside an armchair.
                        ; Pick the table axis facing its center, rather than shrinking
                        ; its support around the actor's unrelated seated heading.
                        float axisSin = Math.Sin(angle)
                        float axisCos = Math.Cos(angle)
                        float centerX = (spec[0] + spec[2]) * 0.5
                        float centerY = (spec[1] + spec[3]) * 0.5
                        float centerDX = dx + scale * (centerX * axisCos + centerY * axisSin)
                        float centerDY = dy + scale * (0.0 - centerX * axisSin + centerY * axisCos)
                        float along = centerDX * axisSin + centerDY * axisCos
                        float across = centerDX * axisCos - centerDY * axisSin
                        if Math.Abs(across) > Math.Abs(along)
                            if across >= 0.0
                                angle += 90.0
                            else
                                angle -= 90.0
                            endif
                        elseif along < 0.0
                            angle += 180.0
                        endif
                        while angle >= 360.0
                            angle -= 360.0
                        endwhile
                        while angle < 0.0
                            angle += 360.0
                        endwhile
                        if distance < bestDistance
                            bestDistance = distance
                            frame = angle
                            if LocalDiagnostics
                                Debug.Trace("[InnOrdering 2.0] lowtable_sideframe1 profile=" + index + " actorFacing=" + facing + " servingFrame=" + frame)
                            endif
                        endif
                    else
                        ; Preserve the original facing rule for ordinary tables.
                        float difference = facing - angle
                        while difference > 45.0
                            angle += 90.0
                            difference -= 90.0
                        endwhile
                        while difference < -45.0
                            angle -= 90.0
                            difference += 90.0
                        endwhile
                        if Math.Abs(difference) < 12.0 && distance < bestDistance && forward > 15.0
                            bestDistance = distance
                            frame = angle
                        endif
                    endif
                    n += 1
                endif
        endif
        if LocalSearchExpired(started)
            return LocalSearchTimedOut(started, "table snapshot")
        endif
        i += 1
    endwhile
    LocalYaw = frame
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Tables cached=" + n + " elapsed=" + (Utility.GetCurrentRealTime() - started))
    endif
    pc = Math.Cos(frame)
    ps = Math.Sin(frame)
    ; Rectangles in seat coordinates: sideways S, forward F.
    float[] loS = new float[128]
    float[] hiS = new float[128]
    float[] loF = new float[128]
    float[] hiF = new float[128]
    float[] top = new float[128]
    bool[] isBar = new bool[128]
    ObjectReference[] rectangleTables = new ObjectReference[128]
    int count = 0
    i = 0
    while i < n
        float sn = Math.Sin(angles[i] - frame)
        float cs = Math.Cos(angles[i] - frame)
        float asin = Math.Abs(sn)
        float acos = Math.Abs(cs)
        if scales[i] > 0.1
            int part = 0
            int parts = 1
            if profiles[i] == 33
                parts = 2
            endif
            while part < parts
                float[] spec = LocalTableSpec(profiles[i])
                if profiles[i] == 33
                    ; CounterCornerIn is L-shaped: split its two arms.
                    if part == 0
                        spec[2] = 32.0
                    else
                        spec[0] = 32.0
                        spec[1] = -32.0
                    endif
                endif
                float scale = scales[i]
                float cx = (spec[0] + spec[2]) * 0.5
                float cy = (spec[1] + spec[3]) * 0.5
                float hx = (spec[2] - spec[0]) * 0.5
                float hy = (spec[3] - spec[1]) * 0.5
                float centerS = xs[i] * pc - ys[i] * ps + scale * (cx * cs + cy * sn)
                float centerF = xs[i] * ps + ys[i] * pc + scale * (0.0 - cx * sn + cy * cs)
                ; A rotated rectangle contributes an INSCRIBED support rectangle.
                ; Its enclosing AABB would falsely include unsupported corner space.
                float outerWidth = hx * acos + hy * asin
                float outerDepth = hx * asin + hy * acos
                float supportFactor = LocalMin(hx / (outerWidth * acos + outerDepth * asin), hy / (outerWidth * asin + outerDepth * acos))
                float width = scale * outerWidth * supportFactor
                float depth = scale * outerDepth * supportFactor
                if PullUpAChairOrdering.IsRoundTable(profiles[i])
                    ; A circle's safe square is independent of the table reference's yaw.
                    width = scale * LocalMin(hx, hy)
                    depth = width
                endif
                loS[count] = centerS - width
                hiS[count] = centerS + width
                loF[count] = centerF - depth
                hiF[count] = centerF + depth
                top[count] = heights[i]
                rectangleTables[count] = tableRefs[i]
                isBar[count] = PullUpAChairOrdering.IsCounter(profiles[i])
                count += 1
                part += 1
            endwhile
        endif
        i += 1
    endwhile
    ; Cache strip merges lazily, preserving the default layout's fast exit.
    bool[] supportReady = new bool[128]
    float[] supportLeft = new float[128]
    float[] supportRight = new float[128]
    float[] supportNear = new float[128]
    float[] supportFar = new float[128]
    float[] widths = new float[3]
    float[] depths = new float[3]
    float[] heightsFood = new float[3]
    float[] slotSides = new float[3]
    float[] slotForwards = new float[3]
    float minSide = 1000.0
    float maxSide = -1000.0
    float minForward = 1000.0
    float maxForward = -1000.0
    i = 0
    while i < dishCount
        int foodIndex = PullUpAChairOrdering.FoodIndex(foodList, dishes[i])
        widths[i] = LocalFoodWidth(foodIndex) * LocalSlotScale(i)
        depths[i] = LocalFoodDepth(foodIndex) * LocalSlotScale(i)
        float[] bounds = LocalFoodBounds(foodIndex)
        heightsFood[i] = (bounds[5] - bounds[2]) * LocalSlotScale(i) + 0.5
        if i == 0
            widths[i] = LocalMainWidth
            depths[i] = LocalMainDepth
            heightsFood[i] = (bounds[5] - bounds[2]) * LocalMainFoodScale + 0.5
            heightsFood[i] += LocalMainRise
            if LocalHasPlate
                heightsFood[i] = LocalMax(heightsFood[i], 3.81426 * LocalPlateScale + 0.45)
            endif
        endif
        i += 1
    endwhile
    float[] layoutSides = new float[12]
    float[] layoutForwards = new float[12]
    int layout = 0
    while layout < 4
        float[] offsets = LocalLayoutOffsets(layout, widths, depths)
        i = 0
        while i < dishCount
            float sideways = offsets[i * 2]
            float forward = offsets[i * 2 + 1]
            layoutSides[layout * 3 + i] = sideways
            layoutForwards[layout * 3 + i] = forward
            minSide = LocalMin(minSide, sideways - widths[i])
            maxSide = LocalMax(maxSide, sideways + widths[i])
            minForward = LocalMin(minForward, forward - depths[i])
            maxForward = LocalMax(maxForward, forward + depths[i])
            i += 1
        endwhile
        layout += 1
    endwhile
    ; A conservative union of every possible serving footprint, including collision margins.
    float[] envelope = new float[6]
    envelope[0] = minSide - 57.0
    envelope[1] = maxSide + 57.0
    envelope[2] = minForward + 29.0
    envelope[3] = maxForward + 106.0
    envelope[4] = 1000000.0
    envelope[5] = -1000000.0
    float tallest = LocalMax(heightsFood[0], LocalMax(heightsFood[1], heightsFood[2]))
    i = 0
    while i < count
        envelope[4] = LocalMin(envelope[4], top[i])
        envelope[5] = LocalMax(envelope[5], top[i] + tallest)
        i += 1
    endwhile
    ; Clutter is read only if a geometric placement exists, once for the order.
    float[] clutterX = new float[128]
    float[] clutterY = new float[128]
    float[] clutterZ = new float[128]
    float[] clutterWidth = new float[128]
    float[] clutterDepth = new float[128]
    float[] clutterTop = new float[128]
    int clutterCount = -1
    bool footprintExists = false
    int candidatesChecked = 0
    int candidatesSkipped = 0
    float bestScore = 1000000.0
    float bestS
    float bestF
    float bestZ
    layout = 0
    while layout < 4
        minSide = 1000.0
        maxSide = -1000.0
        minForward = 1000.0
        maxForward = -1000.0
        i = 0
        while i < dishCount
            slotSides[i] = layoutSides[layout * 3 + i]
            slotForwards[i] = layoutForwards[layout * 3 + i]
            minSide = LocalMin(minSide, slotSides[i] - widths[i])
            maxSide = LocalMax(maxSide, slotSides[i] + widths[i])
            minForward = LocalMin(minForward, slotForwards[i] - depths[i])
            maxForward = LocalMax(maxForward, slotForwards[i] + depths[i])
            i += 1
        endwhile
        LocalDrinkSide = slotSides[1]
        LocalExtraSide = slotSides[2]
        LocalDrinkForward = slotForwards[1]
        LocalExtraForward = slotForwards[2]
        if LocalDiagnostics
            Debug.Trace("[InnOrdering 2.0] Trying layout=" + layout)
        endif
        bestScore = 1000000.0
        i = 0
        while i < count
            float left = supportLeft[i]
            float right = supportRight[i]
            float near = supportNear[i]
            float far = supportFar[i]
            if !supportReady[i]
                left = loS[i]
                right = hiS[i]
                near = loF[i]
                far = hiF[i]
                if isBar[i]
                    int pass = 0
                    bool changed = true
                    while changed && pass < 8
                        changed = false
                        int j = 0
                        while j < count
                            if isBar[j] && Math.Abs(top[j] - top[i]) < 0.5 && loS[j] <= right + 0.03 && hiS[j] >= left - 0.03
                                float crossNear = LocalMax(near, loF[j])
                                float crossFar = LocalMin(far, hiF[j])
                                float minimumDepth = LocalMin(far - near, hiF[j] - loF[j])
                                if crossFar - crossNear >= minimumDepth - 2.5 && crossFar - crossNear > 40.0
                                    if loS[j] < left - 0.03 || hiS[j] > right + 0.03
                                        left = LocalMin(left, loS[j])
                                        right = LocalMax(right, hiS[j])
                                        near = crossNear
                                        far = crossFar
                                        changed = true
                                    endif
                                endif
                            endif
                            j += 1
                        endwhile
                        pass += 1
                        if LocalSearchExpired(started)
                            if bestScore < 1000000.0
                                return LocalAcceptSpot(px, py, bestS, bestF, bestZ, started)
                            endif
                            return LocalSearchTimedOut(started, "strip merge")
                        endif
                    endwhile
                endif
                supportLeft[i] = left
                supportRight[i] = right
                supportNear[i] = near
                supportFar[i] = far
                supportReady[i] = true
            endif
            ; Solve the valid center interval directly, including lateral adjustment
            ; for seats at an end or a corner. There is no 25-step position scan.
            float sideLow = LocalMax(-56.0, left - minSide + 0.5)
            float sideHigh = LocalMin(56.0, right - maxSide - 0.5)
            float forwardLow = LocalMax(30.0, near - minForward + 0.5)
            float forwardHigh = LocalMin(105.0, far - maxForward - 0.5)
            if sideLow <= sideHigh && forwardLow <= forwardHigh
                footprintExists = true
                if clutterCount < 0
                    clutterCount = LocalCacheClutter(clutterX, clutterY, clutterZ, clutterWidth, clutterDepth, clutterTop, room, started, foodList, px, py, envelope)
                    if clutterCount < 0
                        return LocalSearchTimedOut(started, "clutter snapshot")
                    endif
                endif
                float preferredS = LocalMin(sideHigh, LocalMax(sideLow, 0.0))
                float preferredF = LocalMin(forwardHigh, LocalMax(forwardLow, 55.0))
                int attempt = 0
                float minimumOffsetF = preferredF - 55.0
                if preferredS * preferredS + minimumOffsetF * minimumOffsetF >= bestScore
                    ; Even the closest point in this rectangle cannot improve the current result.
                    candidatesSkipped += 21
                    attempt = 21
                endif
                while attempt < 21
                    float centerS = preferredS
                    float centerF = preferredF
                    int sideChoice = attempt % 7
                    int forwardChoice = attempt / 7
                    if sideChoice == 1
                        centerS -= 12.0
                    elseif sideChoice == 2
                        centerS += 12.0
                    elseif sideChoice == 3
                        centerS -= 24.0
                    elseif sideChoice == 4
                        centerS += 24.0
                    elseif sideChoice == 5
                        centerS = sideLow
                    elseif sideChoice == 6
                        centerS = sideHigh
                    endif
                    centerS = LocalMin(sideHigh, LocalMax(sideLow, centerS))
                    if forwardChoice == 1
                        centerF = forwardLow
                    elseif forwardChoice == 2
                        centerF = forwardHigh
                    endif
                    float offsetF = centerF - 55.0
                    float score = centerS * centerS + offsetF * offsetF
                    if score < bestScore
                        candidatesChecked += 1
                        bool clear = true
                        int slot = 0
                        while slot < dishCount && clear
                            float s = centerS + slotSides[slot]
                            float f = centerF + slotForwards[slot]
                            float dishTop = top[i] + heightsFood[slot]
                            int j = 0
                            while j < clutterCount && clear
                                if clutterTop[j] > top[i] + 0.15 && clutterZ[j] < dishTop
                                    float ds = clutterX[j] - s
                                    float df = clutterY[j] - f
                                    float allowedS = widths[slot] + clutterWidth[j] + 1.0
                                    float allowedF = depths[slot] + clutterDepth[j] + 1.0
                                    if ds > 0.0 - allowedS && ds < allowedS && df > 0.0 - allowedF && df < allowedF
                                        clear = false
                                    endif
                                endif
                                j += 1
                            endwhile
                            slot += 1
                        endwhile
                        if clear
                            LocalTable = rectangleTables[i]
                            bestScore = score
                            bestS = centerS
                            bestF = centerF
                            bestZ = top[i]
                            if bestScore == 0.0
                                if LocalDiagnostics
                                    Debug.Trace("[InnOrdering 2.0] Optimal center found; candidates checked=" + candidatesChecked + " skipped=" + candidatesSkipped)
                                endif
                                return LocalAcceptSpot(px, py, bestS, bestF, bestZ, started)
                            endif
                            ; This is the closest point inside this support rectangle; later points cannot improve it.
                            if centerS == preferredS && centerF == preferredF
                                attempt = 21
                            endif
                        endif
                    else
                        candidatesSkipped += 1
                    endif
                    if attempt % 7 == 6 && LocalSearchExpired(started)
                        if bestScore < 1000000.0
                            return LocalAcceptSpot(px, py, bestS, bestF, bestZ, started)
                        endif
                        return LocalSearchTimedOut(started, "candidate checks")
                    endif
                    attempt += 1
                endwhile
            endif
            if LocalSearchExpired(started)
                if bestScore < 1000000.0
                    i = count
                else
                    return LocalSearchTimedOut(started, "geometry")
                endif
            endif
            i += 1
        endwhile
        if bestScore < 1000000.0
            if LocalDiagnostics
                Debug.Trace("[InnOrdering 2.0] Selected layout=" + layout + " candidates=" + candidatesChecked)
            endif
            return LocalAcceptSpot(px, py, bestS, bestF, bestZ, started)
        endif
        layout += 1
    endwhile
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Rejected rects=" + count + " footprint=" + footprintExists + " elapsed=" + (Utility.GetCurrentRealTime() - started))
    endif
    if footprintExists
        Debug.Notification(PullUpAChairOrdering.Text("occupied"))
    else
        Debug.Notification(PullUpAChairOrdering.Text("no_space"))
    endif
    return false
EndFunction

int Function LocalCacheClutter(float[] xs, float[] ys, float[] bottoms, float[] widths, float[] depths, float[] tops, Cell room, float started, FormList foodList, float px, float py, float[] envelope)
    Form sampleFood = Game.GetFormFromFile(0x64B2E, "Skyrim.esm")
    FormList statics = Game.GetFormFromFile(0x802, "Pull up a chair Ordering.esp") as FormList
    FormList props = Game.GetFormFromFile(0x803, "Pull up a chair Ordering.esp") as FormList
    if !statics || !props
        return -1
    endif
    float pc = Math.Cos(LocalYaw)
    float ps = Math.Sin(LocalYaw)
    int kind = 0
    int count = 0
    int examined = 0
    int broadRejected = 0
    int upright = 0
    while kind < 3
        ObjectReference[] refs
        if kind == 0
            refs = PO3_SKSEFunctions.FindAllReferencesOfFormType(PlayerRef, sampleFood.GetType(), 200.0)
        elseif kind == 1
            refs = PO3_SKSEFunctions.FindAllReferencesOfFormType(PlayerRef, Gold001.GetType(), 200.0)
        else
            refs = PO3_SKSEFunctions.FindAllReferencesOfType(PlayerRef, statics, 200.0)
        endif
        int i = 0
        while refs && i < refs.Length
            ObjectReference item = refs[i]
            if item && !item.IsDisabled() && item.GetParentCell() == room
                examined += 1
                Form base = item.GetBaseObject()
                int index
                float[] spec
                if kind == 0
                    index = PullUpAChairOrdering.FoodIndex(foodList, base)
                    spec = LocalFoodBounds(index)
                else
                    index = props.Find(base)
                    spec = LocalPropBounds(index)
                endif
                float scale = item.GetScale()
                if index < 0
                    ; Always overwrite the array for unknown clutter; never reuse a previous item's bounds.
                    float halfWidth = item.GetWidth() * 0.5
                    float halfDepth = item.GetLength() * 0.5
                    float height = item.GetHeight()
                    if scale > 0.1
                        halfWidth /= scale
                        halfDepth /= scale
                        height /= scale
                    endif
                    spec[0] = 0.0 - LocalMax(halfWidth, 2.0)
                    spec[1] = 0.0 - LocalMax(halfDepth, 2.0)
                    spec[2] = 0.0 - height * 0.5
                    spec[3] = 0.0 - spec[0]
                    spec[4] = 0.0 - spec[1]
                    spec[5] = height
                endif
                float dx = item.GetPositionX() - px
                float dy = item.GetPositionY() - py
                float z = item.GetPositionZ()
                float pivotS = dx * pc - dy * ps
                float pivotF = dx * ps + dy * pc
                ; A sphere enclosing the whole box can safely reject distant clutter before angle queries.
                float rx = LocalMax(0.0 - spec[0], spec[3])
                float ry = LocalMax(0.0 - spec[1], spec[4])
                float rz = LocalMax(0.0 - spec[2], spec[5])
                float radius = Math.Sqrt(rx * rx + ry * ry + rz * rz) * scale
                if pivotS + radius >= envelope[0] && pivotS - radius <= envelope[1] && pivotF + radius >= envelope[2] && pivotF - radius <= envelope[3] && z + radius >= envelope[4] && z - radius <= envelope[5]
                    if count >= 128
                        return -1
                    endif
                    float cx = (spec[0] + spec[3]) * 0.5 * scale
                    float cy = (spec[1] + spec[4]) * 0.5 * scale
                    float cz = (spec[2] + spec[5]) * 0.5 * scale
                    float hx = (spec[3] - spec[0]) * 0.5 * scale
                    float hy = (spec[4] - spec[1]) * 0.5 * scale
                    float hz = (spec[5] - spec[2]) * 0.5 * scale
                    float ax = item.GetAngleX()
                    float ay = item.GetAngleY()
                    float az = item.GetAngleZ() - LocalYaw
                    float sn = Math.Sin(az)
                    float cs = Math.Cos(az)
                    if ax == 0.0 && ay == 0.0
                        ; Exact yaw-only transform for upright dishes; tilted clutter uses the full matrix below.
                        float absSn = Math.Abs(sn)
                        float absCs = Math.Abs(cs)
                        xs[count] = pivotS + cs * cx + sn * cy
                        ys[count] = pivotF - sn * cx + cs * cy
                        widths[count] = absCs * hx + absSn * hy
                        depths[count] = absSn * hx + absCs * hy
                        bottoms[count] = z + spec[2] * scale
                        tops[count] = z + spec[5] * scale
                        upright += 1
                    else
                        float sx = Math.Sin(ax)
                        float sy = Math.Sin(ay)
                        float cosX = Math.Cos(ax)
                        float cosY = Math.Cos(ay)
                        float m00 = cosY * cs
                        float m01 = cosX * sn + sx * sy * cs
                        float m02 = sx * sn - cosX * sy * cs
                        float m10 = 0.0 - cosY * sn
                        float m11 = cosX * cs - sx * sy * sn
                        float m12 = sx * cs + cosX * sy * sn
                        float m20 = sy
                        float m21 = 0.0 - sx * cosY
                        float m22 = cosX * cosY
                        xs[count] = pivotS + m00 * cx + m01 * cy + m02 * cz
                        ys[count] = pivotF + m10 * cx + m11 * cy + m12 * cz
                        widths[count] = Math.Abs(m00) * hx + Math.Abs(m01) * hy + Math.Abs(m02) * hz
                        depths[count] = Math.Abs(m10) * hx + Math.Abs(m11) * hy + Math.Abs(m12) * hz
                        float middleZ = z + m20 * cx + m21 * cy + m22 * cz
                        float halfZ = Math.Abs(m20) * hx + Math.Abs(m21) * hy + Math.Abs(m22) * hz
                        bottoms[count] = middleZ - halfZ
                        tops[count] = middleZ + halfZ
                    endif
                    ; Keep only boxes that could overlap any supported candidate, with collision margins included.
                    if xs[count] + widths[count] >= envelope[0] && xs[count] - widths[count] <= envelope[1] && ys[count] + depths[count] >= envelope[2] && ys[count] - depths[count] <= envelope[3] && tops[count] >= envelope[4] && bottoms[count] <= envelope[5]
                        count += 1
                    else
                        broadRejected += 1
                    endif
                else
                    broadRejected += 1
                endif
            endif
            if i % 4 == 3 && LocalSearchExpired(started)
                return -1
            endif
            i += 1
        endwhile
        if LocalSearchExpired(started)
            return -1
        endif
        kind += 1
    endwhile
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Clutter examined=" + examined + " cached=" + count + " distant=" + broadRejected + " upright=" + upright + " elapsed=" + (Utility.GetCurrentRealTime() - started))
    endif
    return count
EndFunction

bool Function LocalSearchExpired(float started)
    float now = Utility.GetCurrentRealTime()
    ; A saved in-flight search must also cancel when a new game process resets real time.
    return now < started || now - started > 12.0
EndFunction

bool Function LocalSearchTimedOut(float started, string phase)
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Cancelled phase=" + phase + " elapsed=" + (Utility.GetCurrentRealTime() - started))
    endif
    Debug.Notification(PullUpAChairOrdering.Text("search_timeout"))
    return false
EndFunction

bool Function LocalClearSpot(float x, float y, float z, float radius, ObjectReference[] clutter)
    int i = 0
    while clutter && i < clutter.Length
        ObjectReference item = clutter[i]
        if item && !item.IsDisabled() && item.Is3DLoaded() && item.GetParentCell() == PlayerRef.GetParentCell()
            float dz = Math.Abs(item.GetPositionZ() - z)
            if dz < 30.0
                float dx = item.GetPositionX() - x
                float dy = item.GetPositionY() - y
                float exclusion = radius + 10.0
                if dx * dx + dy * dy < exclusion * exclusion
                    return false
                endif
            endif
        endif
        i += 1
    endwhile
    return true
EndFunction

Function LocalDiscardUnpaid()
    int i = 0
    while LocalMeal && i < LocalMeal.Length
        if LocalMeal[i]
            LocalMeal[i].DisableNoWait()
            LocalMeal[i].Delete()
            LocalMeal[i] = None
        endif
        i += 1
    endwhile
    LocalMealCell = None
EndFunction

Function LocalPackMeal()
    ; Compatibility stub. No packing, item movement, deletion, or polling.
    UnregisterForUpdate()
    LocalMeal = new ObjectReference[4]
    LocalMealCell = None
EndFunction

Event OnUpdate()
    ; Consume a pending callback from earlier versions without touching any item.
    UnregisterForUpdate()
    if !LocalBusy
        LocalMeal = new ObjectReference[4]
        LocalMealCell = None
    endif
EndEvent

float[] Function LocalTableSpec(int index)
    return PullUpAChairOrdering.TableSpec(index)
EndFunction

float Function LocalFoodBottom(int index)
    return PullUpAChairOrdering.ProfileValue(index, 1)
EndFunction

int Function LocalFoodCourse(int index)
    return PullUpAChairOrdering.ProfileValue(index, 0) as Int
EndFunction

float Function LocalFoodRadius(int index)
    return PullUpAChairOrdering.ProfileValue(index, 4)
EndFunction

bool Function LocalAcceptSpot(float px, float py, float sideways, float forward, float height, float started)
    LocalX = px + sideways * Math.Cos(LocalYaw) + forward * Math.Sin(LocalYaw)
    LocalY = py - sideways * Math.Sin(LocalYaw) + forward * Math.Cos(LocalYaw)
    LocalZ = height
    if LocalDiagnostics
        Debug.Trace("[InnOrdering 2.0] Accepted supported clear candidate; elapsed=" + (Utility.GetCurrentRealTime() - started))
    endif
    return true
EndFunction

float[] Function LocalFoodBounds(int index)
    return PullUpAChairOrdering.ProfileBounds(index)
EndFunction

float[] Function LocalPropBounds(int index)
    return PullUpAChairOrdering.PropBounds(index)
EndFunction

float Function LocalFoodWidth(int index)
    return PullUpAChairOrdering.ProfileValue(index, 2)
EndFunction

float Function LocalFoodDepth(int index)
    return PullUpAChairOrdering.ProfileValue(index, 3)
EndFunction

float Function LocalSlotScale(int slot)
    if slot < 0 || slot > 2
        return 1.0
    endif
    if LocalFoodScales && LocalFoodScales.Length == 3
        if LocalFoodScales[slot] > 0.0 && LocalFoodScales[slot] <= 1.0
            return LocalFoodScales[slot]
        endif
    endif
    ; Compatibility for an older saved serving stack without the new scale array.
    if slot == 0 && LocalMainFoodScale > 0.0 && LocalMainFoodScale <= 1.0
        return LocalMainFoodScale
    endif
    return 1.0
EndFunction

float Function LocalPlateScaleFor(int index)
    return PullUpAChairOrdering.ProfileValue(index, 5)
EndFunction

float Function LocalPlateSupportFor(int index)
    return PullUpAChairOrdering.ProfileValue(index, 6)
EndFunction
