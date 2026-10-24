{
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "shooter",
  "startNodeId": "intro",
  "nodes": [
    {
      "id": "intro",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Удержите рубеж. WASD — движение, ЛКМ — огонь, R — перезарядка, H — аптечка. 1/2 — выбор оружия.",
      "nextNodeId": "start"
    },
    {
      "id": "start",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "95a178c2-bcdf-5e20-9ae9-6209f8df34da"
        },
        "Action": "StartBattle"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "combat"
        }
      ]
    },
    {
      "id": "combat",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Бой начался. Можно продолжить реплику: бой продолжится.",
      "nextNodeId": "battle"
    },
    {
      "id": "battle",
      "kind": "Narrative",
      "narrativeType": "game.TemplateAction",
      "inputs": {
        "Controller": {
          "object": "95a178c2-bcdf-5e20-9ae9-6209f8df34da"
        },
        "Action": "WaitBattle"
      },
      "sources": {},
      "choices": [
        {
          "id": "Done",
          "label": "Done",
          "nextNodeId": "victory"
        },
        {
          "id": "Defeat",
          "label": "Defeat",
          "nextNodeId": "defeat"
        }
      ],
      "text": "Победите цели задания."
    },
    {
      "id": "victory",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Все цели задания побеждены. Рубеж удержан.",
      "nextNodeId": "end"
    },
    {
      "id": "defeat",
      "kind": "Line",
      "speaker": "Рассказчик",
      "text": "Боец погиб. Начните заново.",
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