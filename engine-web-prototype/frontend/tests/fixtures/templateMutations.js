// Substantive edits of the real source, never special runtime behavior by genre.
export const templateMutations=[
 {path:'shooter',component:'Medkit',from:'amount: params.heal',to:'amount: params.heal * 2',result:'Аптечка лечит 70 вместо 35 HP'},
 {path:'point-and-click',component:'LockedDoor',from:'return count > 0',to:'return count >= 2',result:'Один ключ больше не открывает дверь'},
 {path:'novel',component:'Reward',from:'params.base * params.multiplier * args.factor',to:'params.base * params.multiplier * args.factor + 5',result:'Поддержка даёт 6 вместо 1'},
 {path:'ui-clicker',component:'Reward',from:'params.base * params.multiplier * args.factor',to:'params.base * params.multiplier * args.factor + 5',result:'Кнопка и таймер дают дополнительно 5'},
 {path:'letters-after-rain',component:'StoryFlag',from:'value: true',to:'value: false',result:'Осмотр больше не разрешает чтение письма'}
];
