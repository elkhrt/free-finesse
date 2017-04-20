var core = require('./main.js')
assert = require('assert');

function test_1()
{
  let north = 'AQT98';
  let south = '7654';
  let sc = new core.SuitCombination(core.cards_from_string(north), core.cards_from_string(south));
  let lc = new core.LineChecker(sc);
  let ls = new core.LineSolver(lc);
  let t0 = process.hrtime();
  let actual = ls.solve();
  let duration = process.hrtime(t0);
  duration = duration[0]*1000 + duration[1]/1000000;
  let expected = [
      [ 3, 4, 4, 3, 4, 4, 4, 4, 5, 4, 4, 4 ],
      [ 3, 4, 4, 3, 4, 4, 5, 4, 4, 4, 4, 4 ],
      [ 3, 4, 4, 3, 5, 4, 5, 3, 4, 4, 5, 4 ],
      [ 3, 4, 4, 3, 4, 4, 5, 3, 4, 5, 4, 5 ],
      [ 3, 4, 4, 3, 4, 4, 5, 4, 4, 5, 3, 5 ],
      [ 3, 4, 4, 3, 5, 4, 5, 4, 3, 4, 5, 4 ] ];
  assert.deepEqual(expected.sort(), actual.sort());
  let lt = new core.LineTree(sc);
  for (let i = 0; i < expected.length; ++i) {
    let tree = lt.tree_for_line(expected[i]);
    console.log('Line ' + i, tree);
  }
  console.log('Analysis time: ' + duration + ' millisec')
}

test_1();
