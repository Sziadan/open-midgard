var globalThis = this;
if (typeof TextDecoder === 'undefined') {
 globalThis.TextDecoder = function() {};
 globalThis.TextDecoder.prototype.decode = function(bytes) {
  var text='';
  for(var i=0;i<bytes.length;) {
   var a=bytes[i++],value=a,need=0,min=0;
   if(a<128) {text+=String.fromCharCode(a);continue;}
   if(a>=194&&a<=223) {value=a&31;need=1;min=128;}
   else if(a>=224&&a<=239) {value=a&15;need=2;min=2048;}
   else if(a>=240&&a<=244) {value=a&7;need=3;min=65536;}
   else {text+='\uFFFD';continue;}
   var good=true;
   for(var j=0;j<need;j++) {
    if(i>=bytes.length || (bytes[i]&192)!==128) {good=false;break;}
    value=(value<<6)|(bytes[i++]&63);
   }
   if(!good || value<min || value>1114111 || (value>=55296&&value<=57343)) {text+='\uFFFD';continue;}
   if(value<65536)text+=String.fromCharCode(value);
   else {value-=65536;text+=String.fromCharCode(55296+(value>>10),56320+(value&1023));}
  }
  return text;
 };
}
