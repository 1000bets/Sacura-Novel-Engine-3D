import React from "react";
import { AUDIO_ASSETS, TYPES } from "./studioModel.js";

export default function ActionFields({
  action: a,
  project,
  onChange,
  compact = false,
}) {
  const field = (label, control) => (
    <label className="action-field">
      <span>{label}</span>
      {control}
    </label>
  );
  const select = (key, options) => (
    <select
      value={a[key] ?? ""}
      onChange={(e) => onChange({ [key]: e.target.value })}
    >
      {options.map((x) => {
        const [v, l] = Array.isArray(x) ? x : [x, x];
        return (
          <option value={v} key={v}>
            {l}
          </option>
        );
      })}
    </select>
  );
  const number = (key, min = 0) => (
    <input
      type="number"
      min={min}
      step=".1"
      value={a[key] ?? 2}
      onChange={(e) => onChange({ [key]: Number(e.target.value) })}
    />
  );
  const type = a.type,
    duration =
      ["move", "wait", "duck", "stop"].includes(type) ||
      (type === "sound" && !a.assetId);
  return (
    <div className={"typed-action-fields " + (compact ? "compact" : "")}>
      {["move", "pose", "visibility"].includes(type) &&
        field(
          type === "pose" ? "Персонаж" : "Объект",
          select(
            "target",
            project.objects
              .filter((o) => type !== "pose" || o.type === "Персонаж")
              .map((o) => [o.id, o.name]),
          ),
        )}
      {type === "move" &&
        field("К точке", select("value", ["стол", "камин", "окно", "диван"]))}
      {type === "pose" &&
        field(
          "Поза",
          select("value", [
            "стоит",
            "улыбка",
            "задумчивость",
            "танец",
            "грусть",
          ]),
        )}
      {type === "camera" &&
        field("План", select("value", ["Общий план", "Крупный план"]))}
      {type === "weather" &&
        field("Погода", select("value", ["Ясно", "Дождь", "Гроза", "Туман"]))}
      {type === "time" &&
        field(
          "Время суток",
          select("value", ["Рассвет", "День", "Закат", "Ночь"]),
        )}
      {type === "visibility" &&
        field("Видимость", select("value", ["Показать", "Скрыть"]))}
      {["music", "sound"].includes(type) && (
        <>
          {field(
            "Аудиофайл",
            select("assetId", [
              ["", "Без файла"],
              ...AUDIO_ASSETS.filter(
                (s) => type !== "music" || s.kind === "music",
              ).map((s) => [s.id, s.name]),
            ]),
          )}
          {field(
            "Громкость · " +
              Math.round((a.volume ?? (type === "music" ? 0.42 : 1)) * 100) +
              "%",
            <input
              type="range"
              min="0"
              max="1"
              step=".01"
              value={a.volume ?? (type === "music" ? 0.42 : 1)}
              onChange={(e) => onChange({ volume: Number(e.target.value) })}
            />,
          )}
          {!compact && (
            <>
              {type === "music" ? (
                <>
                  <label className="check">
                    <input
                      type="checkbox"
                      checked={a.loop !== false}
                      onChange={(e) => onChange({ loop: e.target.checked })}
                    />
                    Повторять по кругу
                  </label>
                  {field("Плавный вход, сек", number("fade"))}
                </>
              ) : (
                <label className="check">
                  <input
                    type="checkbox"
                    checked={a.duck !== false}
                    onChange={(e) => onChange({ duck: e.target.checked })}
                  />
                  Приглушать музыку под голос
                </label>
              )}
            </>
          )}
        </>
      )}
      {["pause", "resume", "stop", "duck"].includes(type) && (
        <div className="action-target-note">Музыка · основной фон</div>
      )}
      {type === "variable" && (
        <>
          {field(
            "Переменная",
            select("target", Object.keys(project.variables)),
          )}
          {field(
            a.target === "letter" ? "Значение" : "Изменение",
            a.target === "letter" ? (
              select("value", ["true", "false"])
            ) : (
              <input
                value={a.value}
                placeholder="+1"
                onChange={(e) => onChange({ value: e.target.value })}
              />
            ),
          )}
          <small className="resource-note">
            +1 прибавляет; число без знака задаёт значение.
          </small>
        </>
      )}
      {type === "wait" && (
        <>
          {field(
            "Ожидание",
            <select
              value={a.waitFor ? "dependency" : "delay"}
              onChange={(e) =>
                onChange({
                  waitFor:
                    e.target.value === "delay"
                      ? ""
                      : project.events.find((e) => e.id !== a.id)?.id || "",
                })
              }
            >
              <option value="delay">Пауза на время</option>
              <option value="dependency">
                Результат события · диагностика
              </option>
            </select>,
          )}
          {a.waitFor &&
            field(
              "Событие",
              select(
                "waitFor",
                project.events.map((e) => [e.id, e.name]),
              ),
            )}
        </>
      )}
      {duration &&
        field(
          type === "stop" ? "Затухание, сек" : "Длительность, сек",
          number("duration", 0.1),
        )}
      {!compact && (
        <div className="action-completion">
          <span>Продолжение группы</span>
          <strong>
            {TYPES[type]?.completion === "CONTINUOUS"
              ? "После запуска · фон остаётся"
              : type === "sound" && a.assetId
                ? "Когда закончится файл"
                : duration
                  ? "После завершения"
                  : "После применения"}
          </strong>
        </div>
      )}
    </div>
  );
}
