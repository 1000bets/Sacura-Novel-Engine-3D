{
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "quest",
  "startNodeId": "intro",
  "nodes": [
    {
      "id": "intro",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Левая дверь открывается ключом. Правая пока заперта.",
      "nextNodeId": "unlock"
    },
    {
      "id": "unlock",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "1df54135-9ddb-5470-bf7d-2df8a0379ba7"
        },
        "Action": "UnlockWarehouse"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "unlocked"
        }
      ]
    },
    {
      "id": "unlocked",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Смотритель разблокировал склад. Теперь можно открыть правую дверь.",
      "nextNodeId": "next"
    },
    {
      "id": "next",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Замок не сбросился. Следующая реплика отдельно открывает склад.",
      "nextNodeId": "open"
    },
    {
      "id": "open",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "1df54135-9ddb-5470-bf7d-2df8a0379ba7"
        },
        "Action": "OpenWarehouse"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "opened"
        }
      ]
    },
    {
      "id": "opened",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Дверь открыта после завершения анимации.",
      "nextNodeId": "end"
    },
    {
      "id": "end",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Обе двери доступны.",
      "nextNodeId": "finished"
    },
    {
      "id": "finished",
      "kind": "End"
    }
  ]
}