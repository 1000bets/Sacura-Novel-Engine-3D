{
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "Navigation validation",
  "startNodeId": "start",
  "nodes": [
    {
      "id": "start",
      "kind": "Line",
      "speaker": "Проверка навигации",
      "text": "Продолжите, чтобы персонаж обошёл стену и подошёл к точке справа.",
      "nextNodeId": "walk"
    },
    {
      "id": "walk",
      "kind": "Action",
      "actions": [
        {
          "kind": "NavigateTo",
          "targetObjectId": "b2a890f0-a8d7-562f-839c-aece1ae1462e",
          "position": [
            4,
            0,
            0
          ],
          "durationSeconds": 30
        }
      ],
      "nextNodeId": "arrived"
    },
    {
      "id": "arrived",
      "kind": "Line",
      "speaker": "Проверка навигации",
      "text": "Персонаж достиг цели. Далее проверяется подъём по ступеням.",
      "nextNodeId": "stairs"
    },
    {
      "id": "stairs",
      "kind": "Action",
      "actions": [
        {
          "kind": "NavigateTo",
          "targetObjectId": "b2a890f0-a8d7-562f-839c-aece1ae1462e",
          "position": [
            4,
            1,
            -6.4
          ],
          "durationSeconds": 30
        }
      ],
      "nextNodeId": "stairsArrived"
    },
    {
      "id": "stairsArrived",
      "kind": "Line",
      "speaker": "Проверка навигации",
      "text": "Персонаж поднялся по ступеням. Далее проверяется переход между платформами.",
      "nextNodeId": "platforms"
    },
    {
      "id": "platforms",
      "kind": "Action",
      "actions": [
        {
          "kind": "MoveTo",
          "targetObjectId": "b2a890f0-a8d7-562f-839c-aece1ae1462e",
          "position": [
            -3,
            2.6,
            -11
          ],
          "durationSeconds": 0
        },
        {
          "kind": "CameraCut",
          "position": [
            0,
            8,
            -3
          ],
          "rotation": [
            -0.31622777,
            0,
            0,
            0.9486833
          ],
          "durationSeconds": 0
        },
        {
          "kind": "NavigateTo",
          "targetObjectId": "b2a890f0-a8d7-562f-839c-aece1ae1462e",
          "position": [
            3,
            2.5,
            -11
          ],
          "durationSeconds": 30
        }
      ],
      "nextNodeId": "end"
    },
    {
      "id": "end",
      "kind": "End"
    }
  ]
}
