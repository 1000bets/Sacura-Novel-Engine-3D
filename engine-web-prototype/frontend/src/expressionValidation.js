// Validation of the existing blueprint expression data, not a second evaluator.
export const EXPRESSION_OPERATORS=['variable','node','constant','get','itemCount','array','struct','field','at','length','and','or','not','eq','ne','add','sub','mul','div','min','max','lt','lte','gt','gte'];
export function validateExpression(expression,depth=0){
 if(depth>64)throw Error('Выражение слишком глубоко.');
 if(!expression||typeof expression!=='object')return;
 if(Array.isArray(expression)){expression.forEach(x=>validateExpression(x,depth+1));return;}
 if(expression.op){if(!EXPRESSION_OPERATORS.includes(expression.op))throw Error('Неизвестное выражение.');if(expression.op==='node'&&(typeof expression.nodeId!=='string'||!expression.nodeId||expression.port&&expression.port!=='value'))throw Error('Выберите существующий выход данных.');if(expression.op==='get'&&(typeof expression.path!=='string'||!expression.path||expression.path.split('.').some(k=>['__proto__','constructor','prototype'].includes(k))))throw Error('Выберите доступное значение выражения.');}
 if(expression.op==='variable'&&(expression.variable?.kind!=='reference'||expression.variable.type!=='variable'))throw Error('Выберите переменную истории.');if(expression.op==='constant')return;
 for(const value of Object.values(expression))validateExpression(value,depth+1);
}
