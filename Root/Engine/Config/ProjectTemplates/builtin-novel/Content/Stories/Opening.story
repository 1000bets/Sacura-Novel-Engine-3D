{
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "novel",
  "startNodeId": "intro",
  "nodes": [
    {
      "id": "intro",
      "kind": "Choice",
      "speaker": "Рассказчик",
      "prompt": "Как ответить?",
      "choices": [
        {
          "id": "7050b0c9-d938-588d-b467-9e850747e48a",
          "label": "Поддержать",
          "nextNodeId": "support"
        },
        {
          "id": "813c4b58-5217-5f73-ba80-21238b92b68f",
          "label": "Ответить холодно",
          "nextNodeId": "gate"
        }
      ]
    },
    {
      "id": "support",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "ecc05fa8-c5c1-5d76-bb0d-21f6e865305e"
        },
        "Action": "AwardSupport"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "snapshot"
        }
      ]
    },
    {
      "id": "snapshot",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "ecc05fa8-c5c1-5d76-bb0d-21f6e865305e"
        },
        "Action": "Snapshot"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "thanks"
        }
      ]
    },
    {
      "id": "thanks",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Спасибо за поддержку. Я запомню этот момент.",
      "nextNodeId": "gate"
    },
    {
      "id": "gate",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "ecc05fa8-c5c1-5d76-bb0d-21f6e865305e"
        },
        "Action": "CheckRelationship"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "end"
        },
        {
          "id": "Available",
          "label": "Available",
          "nextNodeId": "personal"
        },
        {
          "id": "Unavailable",
          "label": "Unavailable",
          "nextNodeId": "cold"
        }
      ]
    },
    {
      "id": "personal",
      "kind": "Choice",
      "speaker": "Рассказчик",
      "prompt": "Продолжим разговор?",
      "choices": [
        {
          "id": "1ad289a3-631a-5878-9dd1-e6282335aa08",
          "label": "Поддержать ещё раз",
          "nextNodeId": "second"
        },
        {
          "id": "2080ce44-0660-567c-9067-0357b0a1a72a",
          "label": "Попрощаться",
          "nextNodeId": "goodbye"
        }
      ]
    },
    {
      "id": "cold",
      "kind": "Choice",
      "speaker": "Рассказчик",
      "prompt": "Продолжим разговор?",
      "choices": [
        {
          "id": "4e24172b-5499-5bba-9c93-5e0246388d5c",
          "label": "Попрощаться",
          "nextNodeId": "goodbye"
        }
      ]
    },
    {
      "id": "second",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "ecc05fa8-c5c1-5d76-bb0d-21f6e865305e"
        },
        "Action": "AwardSupport"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "warm"
        }
      ]
    },
    {
      "id": "warm",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Теперь доверия ещё больше.",
      "nextNodeId": "end"
    },
    {
      "id": "goodbye",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Разговор окончен.",
      "nextNodeId": "finished"
    },
    {
      "id": "end",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Разговор окончен.",
      "nextNodeId": "finished"
    },
    {
      "id": "finished",
      "kind": "End"
    }
  ]
}