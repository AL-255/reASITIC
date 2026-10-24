// Pin the former flat-record encoder as a byte-protocol reference. Read it
// from its commit so regenerating this evidence never uses the new writer.
import {execFileSync} from 'node:child_process';
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {parseTechnology} from '../web/technology.mjs';
const encoderCommit='a80c3a589ced171bd6046619d32883c95a22a3cc',source=execFileSync('git',['show',encoderCommit+':web/gds.mjs'],{encoding:'utf8'});
const {writeGDS}=await import('data:text/javascript,'+encodeURIComponent(source));
const hash=x=>createHash('sha256').update(x).digest('hex'),fixtures=[],skipped=[];
for(const name of ['creator-coverage','creator-ranges','creator-counts','geometry-edits']){
  const path=`tests/fixtures/original-${name}.json`,raw=readFileSync(path),original=JSON.parse(raw),cases=[];
  for(const c of original.cases){
    const technology=c.technology??'CMOS',text=readFileSync(`web/technology/${technology}.tek`,'utf8'),tech=parseTechnology(text);
    const count=segments=>segments.reduce((n,s)=>n+(s.metal<tech.metals.length?1:2+Math.max(0,s.nx)*Math.max(0,s.ny))+(s.branches?count(s.branches):0),0),estimated=110+64*c.shapes.reduce((n,s)=>n+count(s.segments),0);
    if(estimated>64*1024*1024){skipped.push({path,name:c.name,reason:'Former-encoder capture bounded to 64 MiB; large-grid output has a separate streaming reference check'});continue;}
    const bytes=writeGDS({shapes:c.shapes},tech);cases.push({name:c.name,technology,technology_sha256:hash(text),bytes:bytes.length,sha256:hash(bytes)});
  }
  fixtures.push({path,sha256:hash(raw),cases});
}
writeFileSync('tests/fixtures/gds-compatibility.json',JSON.stringify({encoderCommit,encoderSourceSha256:hash(source),fixtures,skipped},null,2)+'\n');
console.log(fixtures.reduce((n,f)=>n+f.cases.length,0)+' former-encoder GDS byte references captured');
