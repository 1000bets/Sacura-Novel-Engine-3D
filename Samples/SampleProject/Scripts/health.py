from engine import ScriptComponent, register_class, field, method


@register_class(type_id="game.Health")
class Health(ScriptComponent):
    InitialHealth = field(float, default=100.0, display_name="Initial Health", ui_range=(1.0, 10000.0))
    Armor = field(float, default=0.0, display_name="Armor", ui_range=(0.0, 10000.0))

    def OnCreate(self):
        self.CurrentHealth = self.InitialHealth

    @method(Parameters={"Amount": float}, Result=float)
    def TakeDamage(self, Amount):
        self.CurrentHealth = max(0.0, self.CurrentHealth - max(0.0, Amount - self.Armor))
        return self.CurrentHealth

    @method(Result=float)
    def GetHealth(self):
        return self.CurrentHealth
