{
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "Пример Python Narrative",
  "startNodeId": "delay",
  "nodes": [
    {
      "id": "multiply",
      "kind": "Narrative",
      "narrativeType": "game.Multiply",
      "inputs": {
        "Value": 12.0,
        "Multiplier": 3.0
      },
      "sources": {}
    },
    {
      "id": "delay",
      "kind": "Narrative",
      "narrativeType": "game.DelayedValue",
      "inputs": {
        "Seconds": 1.0
      },
      "sources": {
        "Value": [
          "multiply",
          "Result"
        ]
      },
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "branch"
        }
      ]
    },
    {
      "id": "branch",
      "kind": "Narrative",
      "narrativeType": "game.Branch",
      "inputs": {
        "Condition": true
      },
      "sources": {},
      "choices": [
        {
          "id": "True",
          "label": "True",
          "nextNodeId": "result"
        },
        {
          "id": "False",
          "label": "False",
          "nextNodeId": "end"
        }
      ]
    },
    {
      "id": "result",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Вычисление завершено. Переменная LastValue получила значение 36.",
      "nextNodeId": "end"
    },
    {
      "id": "end",
      "kind": "End"
    }
  ],
  "editor": {
    "version": 1,
    "positions": {
      "multiply": {
        "x": 0,
        "y": 0
      },
      "delay": {
        "x": 500,
        "y": 0
      },
      "branch": {
        "x": 1000,
        "y": 0
      },
      "result": {
        "x": 1500,
        "y": 0
      },
      "end": {
        "x": 2000,
        "y": 0
      }
    }
  }
}
