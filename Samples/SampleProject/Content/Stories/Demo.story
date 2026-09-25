{
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "Demo",
  "startNodeId": "line_1",
  "nodes": [
    {
      "id": "line_1",
      "kind": "Line",
      "speaker": "Alice",
      "text": "Hello. The harbor is quiet tonight.",
      "nextNodeId": "line_2"
    },
    {
      "id": "line_2",
      "kind": "Line",
      "speaker": "Alice",
      "text": "I think someone is waiting on the pier.",
      "nextNodeId": "line_3"
    },
    {
      "id": "line_3",
      "kind": "Line",
      "speaker": "Narrator",
      "text": "Rain begins to fall on the cobblestones.",
      "nextNodeId": "choice_1"
    },
    {
      "id": "choice_1",
      "kind": "Choice",
      "prompt": "What do you do?",
      "choices": [
        {
          "label": "Walk toward the anchor",
          "nextNodeId": "action_1"
        },
        {
          "label": "Stay under the awning",
          "nextNodeId": "end_stay"
        }
      ]
    },
    {
      "id": "action_1",
      "kind": "Action",
      "actions": [
        {
          "kind": "MoveTo",
          "targetObjectId": "obj_2",
          "position": [0.0, 0.0, -4.0],
          "durationSeconds": 1.5
        },
        {
          "kind": "CameraCut",
          "targetObjectId": "obj_2",
          "useTargetObjectTransform": true,
          "durationSeconds": 0.35
        }
      ],
      "nextNodeId": "end_outside"
    },
    {
      "id": "end_outside",
      "kind": "End"
    },
    {
      "id": "end_stay",
      "kind": "End"
    }
  ]
}
