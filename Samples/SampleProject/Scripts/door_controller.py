from engine import ScriptComponent, register_class, field


@register_class(type_id="game.DoorController", schema_version=1)
class DoorController(ScriptComponent):
    speed = field(
        float,
        property_id="speed",
        default=2.0,
        serializable=True,
        editable=True,
        display_name="Speed",
        ui_range=(0.0, 100.0),
    )
    open_amount = field(
        float,
        property_id="open_amount",
        default=0.0,
        serializable=True,
        editable=True,
        display_name="Open Amount",
    )

    def on_create(self):
        pass

    def on_update(self, delta_time):
        transform = self.owner.get_local_transform()
        transform.position.x += float(self.speed) * float(delta_time)
        self.owner.set_local_transform(transform)

    def on_destroy(self):
        pass
