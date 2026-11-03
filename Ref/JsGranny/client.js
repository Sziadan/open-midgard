import { parseAll, parseAnimated } from './src/Granny.js';
function bytesFromHex(hex) {
 const result=new Uint8Array(hex.length/2);
 for(let i=0;i<result.length;++i) result[i]=parseInt(hex.substr(i*2,2),16);
 return result;
}
function pixelHex(bytes) {
 const table=[];for(let i=0;i<256;++i)table.push(i.toString(16).padStart(2,'0'));
 let result='';for(let i=0;i<bytes.length;++i)result+=table[bytes[i]];
 return result;
}
export function read(hex,animationOnly) {
 const result=animationOnly?parseAnimated(bytesFromHex(hex).buffer):parseAll(bytesFromHex(hex).buffer);
 const textures=(result.textures||[]).filter(t=>t.imgIdx===0&&t.mipIdx===0).map(t=>({name:t.fromFileName,width:t.width,height:t.height,alpha:t.alpha,pixels:pixelHex(t.pixels)}));
 return JSON.stringify({skeletons:result.skeletons,meshes:result.meshes,textures,animations:result.animations,models:result.models||[]},(key,value)=>ArrayBuffer.isView(value)?Array.from(value):value);
}
