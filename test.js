const v8 = require('v8');
const fs = require('fs');
const {SourceMap} = require('bindings')('sourcemap');
const {SourceMapGenerator} = require('source-map');
var Benchmark = require('benchmark');

// fs.writeFileSync('test.map', JSON.stringify(v8.deserialize(fs.readFileSync('/Users/devongovett/Downloads/esbuild/.parcel-cache/00/8d72735c26cdd2d99fe482cd481f24.blob'))));
// console.log(map);
let map = fs.readFileSync('test.map', 'utf8');


let s = new SourceMap();
s.add(map);
console.log(s.stringify());

// let gen = new SourceMapGenerator();
// for (let mapping of JSON.parse(map).mappings) {
//   gen.addMapping(mapping);
// }

// console.log(gen.toString());

// var suite = new Benchmark.Suite;

// // add tests
// suite.add('C++', function() {
//   let s = new SourceMap();
//   s.add(map);
//   s.stringify();
// })
// .add('JS', function() {
//   let gen = new SourceMapGenerator();
//   for (let mapping of JSON.parse(map).mappings) {
//     gen.addMapping(mapping);
//   }

//   gen.toString();
// })
// // add listeners
// .on('cycle', function(event) {
//   console.log(String(event.target));
// })
// .on('complete', function() {
//   console.log('Fastest is ' + this.filter('fastest').map('name'));
// })
// // run async
// .run({ 'async': true });
