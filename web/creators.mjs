// Original command ids. This adapter formats inputs; all parsing, snapping,
// turn normalization and geometry calculations stay in the C++ kernel.
export const creatorIds=Object.freeze({SQUARE:0,SPIRAL:1,WIRE:2,TRANS:3,CAPACITOR:4,BALUN:5,'3DMIRRORTRANS':10,VIA:14,SYMSQ:16,SYMPOLY:17,MMSQUARE:18,RING:22});
export const creatorAliases=Object.freeze({...creatorIds,
  SQ:0,SQR:0,RECT:0,SP:1,SPI:1,S:1,W:2,T:3,TRANSFORMER:3,TR:3,
  CAPTOR:4,CCAP:4,B:5,BALUNT:5,SYMT:5,V:14,CONTACT:14,
  BALSQ:16,CENTERSQ:16,SYMSQUARE:16,BALPOLY:17,CENTERPOLY:17,SYMSP:17,
  MMSQ:18,SQMM:18,RECTMM:18,HALO:22,CIRCLE:22,CIRC:22});
// Native token normalization uppercases ASCII before its case-sensitive
// catalog lookup. The catalog's mixed-case 3DTrans/Trans3D/TransMIM entries
// consequently cannot be reached by an original command line.
const asciiUpper=text=>text.replace(/[a-z]/g,c=>c.toUpperCase());
export function parseCreatorCommand(command,answers='') {
  const match=typeof command==='string'&&command.match(/^[ \t\r\n\f\v,]*([^ \t\r\n\f\v,]+)(?:[ \t\r\n\f\v,]+([\s\S]*?))?[ \t\r\n\f\v]*$/);
  const name=match&&asciiUpper(match[1]);
  if(!match || !Object.hasOwn(creatorAliases,name))throw Error('Use SQUARE, SPIRAL, WIRE, TRANS, CAPACITOR, BALUN, 3DMIRRORTRANS, VIA, SYMSQ, SYMPOLY, MMSQUARE or RING (or an original creator alias).');
  return {id:creatorAliases[name],keywords:match[2]??'',answers};
}
export function creatorRecipe(kind,p,o={}) {
  const [length,height,width,spacing,turns,radius,sides,metal,exit,angle,inner,secondary]=p;
  const name=o.name||'P',second=o.secondaryName||'S';
  const origin={XORG:o.x??0,YORG:o.y??0,ORIENT:angle,PHASE:o.phase??1};
  const winding={W:width,S:spacing,N:turns,METAL:metal};
  let command,fields;
  switch(kind) {
    case 0:command='SQUARE';fields={NAME:name,LEN:length,WID:height,...winding,EXIT:exit,...origin};break;
    case 1:command='SPIRAL';fields={NAME:name,RAD:radius,SIDES:sides,...winding,...origin};break;
    case 2:command='WIRE';fields={NAME:name,LEN:length,WID:width,METAL:metal,...origin};break;
    case 3:command='SYMSQ';fields={NAME:name,LEN:length,ILEN:inner,...winding,METAL2:exit,...origin};break;
    case 4:command='SYMPOLY';fields={NAME:name,RAD:radius,ILEN:inner,SIDES:sides,...winding,METAL2:exit,...origin};break;
    case 5:command='RING';fields={NAME:name,RAD:radius,W:width,GAP:spacing,SIDES:sides,METAL:metal,...origin};break;
    case 6:command='TRANS';fields={PNAME:name,SNAME:second,LEN:length,WID:height,W:width,S:spacing,NP:turns,NS:secondary,METAL:metal,EXIT:exit,...origin};break;
    case 7:return {id:10,keywords:'',answers:[name,second,`${length} ${height}`,width,spacing,turns,metal,exit,`${o.x??0} ${o.y??0}`,angle,o.phase??1].join('\n')+'\n'};
    case 8:command='MMSQUARE';fields={NAME:name,LEN:length,WID:height,...winding,EXIT:exit,...origin};break;
    case 9:command='VIA';fields={NAME:name,NX:o.nx??2,NY:o.ny??2,VIA:o.via??0,XORG:o.x??0,YORG:o.y??0,PHASE:o.phase??1};break;
    case 10:command='CAPACITOR';fields={NAME:name,LEN:length,WID:height,METAL1:metal,METAL2:exit,...origin};break;
    case 11:command='BALUN';fields={NAME:name,LEN:length,W1:width,W2:o.secondaryWidth??width,S:spacing,N:turns,METAL:metal,METAL2:exit,...origin};break;
    default:throw Error('Unknown winding type');
  }
  return {id:creatorIds[command],keywords:Object.entries(fields).map(([key,value])=>`${key}=${value}`).join(':'),answers:''};
}
