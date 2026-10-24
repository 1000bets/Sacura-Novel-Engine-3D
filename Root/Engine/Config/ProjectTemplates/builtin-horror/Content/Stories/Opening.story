{
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "horror",
  "startNodeId": "intro",
  "nodes": [
    {
      "id": "intro",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Корпус № 6 закрыли тридцать лет назад. Сегодня вам пришло сообщение с собственного номера: «Не оставляй меня на вторую смену». Вы пришли на последний обход.",
      "nextNodeId": "enable"
    },
    {
      "id": "enable",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "869ed17d-3556-58d1-8ea6-3136b70afed5"
        },
        "Action": "BeginExploration"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "explore"
        }
      ]
    },
    {
      "id": "explore",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "869ed17d-3556-58d1-8ea6-3136b70afed5"
        },
        "Action": "WaitEscape"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "escaped"
        }
      ]
    },
    {
      "id": "escaped",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Вы вышли в рассвет. В телефоне одно новое сообщение: «Спасибо. Теперь моя очередь уйти». В журнале за вашей подписью больше нет пустой строки.",
      "nextNodeId": "end"
    },
    {
      "id": "end",
      "kind": "End"
    },
    {
      "id": "finished",
      "kind": "End"
    }
  ]
}