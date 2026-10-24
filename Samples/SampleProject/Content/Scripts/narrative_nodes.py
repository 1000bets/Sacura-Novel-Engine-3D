from engine import NarrativeNode, NarrativeResult, pin, register_node


@register_node(type_id="game.Multiply", label="Умножить", pure=True)
class Multiply(NarrativeNode):
    Inputs = {"Value": pin(float, Default=10.0), "Multiplier": pin(float, Default=2.0)}
    Outputs = {"Result": pin(float)}

    def Evaluate(self, Context, Value, Multiplier):
        return {"Result": Value * Multiplier}


@register_node(type_id="game.Branch", label="Условие")
class Branch(NarrativeNode):
    Inputs = {"Condition": pin(bool, Default=True)}
    Exits = ("True", "False")

    def Execute(self, Context, Condition):
        if Condition:
            return NarrativeResult("True")
        return NarrativeResult("False")


@register_node(type_id="game.DelayedValue", label="Записать значение после ожидания")
class DelayedValue(NarrativeNode):
    Inputs = {"Value": pin(float, Default=0.0), "Seconds": pin(float, Default=1.0)}
    Outputs = {"Result": pin(float)}
    Exits = ("Done",)

    def Execute(self, Context, Value, Seconds):
        yield Context.Wait(Seconds)
        Context.SetVariable("LastValue", Value)
        return NarrativeResult(Result=Value)


@register_node(type_id="game.ApplyDamage", label="Нанести урон")
class ApplyDamage(NarrativeNode):
    Inputs = {"Health": pin("engine.ObjectHandle"), "Amount": pin(float, Default=10.0)}
    Outputs = {"RemainingHealth": pin(float)}
    Exits = ("Alive", "Dead")

    def Execute(self, Context, Health, Amount):
        RemainingHealth = Context.Invoke(Health, "TakeDamage", Amount=Amount)
        if RemainingHealth > 0:
            return NarrativeResult("Alive", RemainingHealth=RemainingHealth)
        return NarrativeResult("Dead", RemainingHealth=RemainingHealth)
