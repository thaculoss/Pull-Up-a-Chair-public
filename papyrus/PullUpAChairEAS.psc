; 中文阅读说明
; EAS 与餐桌适配器的原生接口。无覆盖版本通过 DLL 观察原有调用和脚本状态。
; Function 是可调用函数；Event 是游戏触发的事件；Property 是插件记录绑定的属性。
; Global Native 表示由 DLL 实现，脚本这里只声明接口。

Scriptname PullUpAChairEAS Hidden Native

; 登记一份桌上食物与它的餐盘。
Function RegisterPair(ObjectReference food, ObjectReference plate) global native

; 旧版本兼容声明；无覆盖版本保留空实现，不再依赖插入的开始通知。
Function Begin(Actor diner, ActiveMagicEffect effect) global native

; 旧版本兼容声明；不解析可能已经销毁的原生 ActiveMagicEffect。
Function End(Actor diner, ActiveMagicEffect effect) global native

; 旧版本兼容声明；实际法术来自对原有 RemoteCast 调用的观察。
Function TrackAnimation(Form food, Spell animation) global native

; 查询是否开启诊断输出。
Bool Function IsDiagnosticEnabled() global native
