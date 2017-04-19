let epsilon = 1e-6;

function cards_from_string(str) {
    if (str == 'none') return new Set();
    else return new Set(Array.from(str).map(ch => "23456789TJQKA".indexOf(ch)));
}

function cards_to_string(cards) {
    if (!cards.length) return '';
    return [...cards].sort((x, y) => y - x).map(c => '23456789TJQKA'[c]).join('');
}

function nCr(n, r) {
    for (var rv = 1, i = 0; i < r; i++)
        rv = (rv * (n - i)) / (i + 1);
    return rv
}

function canonical_splits(groups) {
    function* partial(i, count, e, w) {
        if (i == groups.length)
            yield [e, w, count];
        else for (let j = 0; j <= groups[i]; j++)
            yield* partial(i + 1, count * nCr(groups[i], j), e.concat([j]), w.concat([groups[i]-j]))
    }
    return Array.from(partial(0, 1, [], [])).sort((a, b) => sum(b[0]) - sum(a[0]));
}

function groups(N, S) {
    for (var i = 0, groups = []; i < 13; ++i) {
        owner = N.has(i) ? 0 : (S.has(i) ? 1 : 2);
        if (i == 0 || groups[groups.length-1][owner] == 0) groups.push([0, 0, 0]);
        groups[groups.length-1][owner]++;
    }
    return groups;
}

function split_prob(ewc) {
    let e = ewc[0].reduce((a, b) => a + b, 0);
    let w = ewc[1].reduce((a, b) => a + b, 0);
    let c = ewc[2];
    return c * nCr(26 - e - w, 13 - e) / 10400600;
}

function sum(arr) {
    rv = arr.reduce((a, b) => a + b, 0);
    return rv;
}

class SuitCombination
{
    constructor(N, S) {
        this.groups = groups(N, S);
        this.splits = canonical_splits(this.groups.map(g => g[2]));
        this.prob = this.splits.map(split_prob);
    }

    group_to_card(group) {
        let c = 0;
        for (let g = 0; g < group; ++g) {
            c += this.groups[g][0] + this.groups[g][1] + this.groups[g][2];
        }
        return '23456789TJQKA'[c];
    }

    split_to_cards(split) {
        let e = [];
        let w = [];
        let c = 0;
        for (let g = 0; g < this.groups.length; ++g) {
            for (let i = 0; i < split[0][g]; ++i) e.push(c++);
            for (let i = 0; i < split[1][g]; ++i) w.push(c++);
            c += this.groups[g][0] + this.groups[g][1];
        }
        return [e, w];
    }

    split_to_string(split) {
        let cards = this.split_to_cards(split);
        return cards_to_string(cards[1]) + ' - ' + cards_to_string(cards[0]);
    }
}

class LineChecker
{
    constructor(suit_combination) {
        this.num_groups = suit_combination.groups.length;
        this.dealt_ns = [suit_combination.groups.map(g => g[0]), 
                         suit_combination.groups.map(g => g[1])];
        this.dealt_ew = suit_combination.splits.map(tc => [tc[0], tc[1]]);
        this.tricks_total = Math.max(sum(this.dealt_ns[0]), sum(this.dealt_ns[1]));        
    }

    check_p1(targets_in, turn) {
        let targets = targets_in.filter(lt => (lt[1] > 0)
            && (Math.max(sum(lt[0][0]), sum(lt[0][1])) + lt[1] > this.tricks_total));
        // TODO - better filtering
        if (!targets.length) return true;
        if (targets.some(lt => lt[1] + Math.floor(turn / 4) > this.tricks_total)) return false;
        for (let play = 0; play < this.num_groups; ++play) {
            for (let player = 0; player < 2; ++player) {
                if (this.played_ns[player][play] < this.dealt_ns[player][play]) {
                    this.plays[turn] = play;
                    this.played_ns[player][play]++;
                    let counter = this.check_p2(targets, turn+1, player);
                    this.played_ns[player][play]--;
                    if (!counter) return true;
                }
            }
        }
        return false;        
    }

    check_p2(targets_in, turn, player) {
        for (let play = 0; play < this.num_groups; ++play) {
            let remaining_layouts = targets_in.filter(lt => lt[0][player][play] > this.played_ew[player][play]);
            if (remaining_layouts.length) {
                this.plays[turn] = play;
                this.played_ew[player][play]++;
                let counter = this.check_p3(remaining_layouts, turn+1, 1-player);
                this.played_ew[player][play]--;
                if (!counter) return true;
            }
        }
        let void_layouts = targets_in.filter(lt => sum(lt[0][player]) <= Math.floor(turn / 4));
        if (void_layouts.length) {
            this.plays[turn] = -1;
            return !this.check_p3(void_layouts, turn+1, 1-player);
        }
        return false;
    }

    check_p3(targets, turn, player) {
        if (this.is_void_NS(player)) {
            this.plays[turn] = -1;
            return !check_p4(targets_in, turn+1, player);
        } else {
            for (let play = 0; play < this.num_groups; ++play) {
                if (this.played_ns[player][play] < this.dealt_ns[player][play]) {
                    this.plays[turn] = play;
                    this.played_ns[player][play]++;
                    let counter = this.check_p4(targets, turn+1, player);
                    this.played_ns[player][play]--;
                    if (!counter) return true;
                }
            }
            return false;
        }
    }  

    check_p4(targets_in, turn, player) {
        for (let play = 0; play < this.num_groups; ++play) {
            let remaining_layouts = targets_in.filter(lt => lt[0][player][play] > this.played_ew[player][play]);
            if (remaining_layouts.length) {
                this.plays[turn] = play;
                this.played_ew[player][play]++;
                let counter = this.check_p5(remaining_layouts, turn+1);
                this.played_ew[player][play]--;
                if (!counter) return true;
            }
        }
        let void_layouts = targets_in.filter(lt => sum(lt[0][player]) <= Math.floor(turn / 4));
        if (void_layouts.length) {
            this.plays[turn] = -1;
            return !this.check_p5(void_layouts, turn+1);
        }
        return false;
    }

    is_void_NS(player) {
        for (let play = 0; play < this.num_groups; ++play)
            if (this.played_ns[player][play] < this.dealt_ns[player][play]) return false;
        return true;
    }

    check_p5(layouts, turn) {
        let i = turn - 4;
        let is_ns_trick = (this.plays[i+0] > this.plays[i+1] && this.plays[i+0] > this.plays[i+3])
                        || (this.plays[i+2] > this.plays[i+1] && this.plays[i+2] > this.plays[i+3]);
        let next_layouts = layouts.map(lt => [lt[0], lt[1] - is_ns_trick]);
        return this.check_p1(next_layouts, turn);
    }

    feasible(trick_targets) {
        this.played_ns = [new Array(this.num_groups).fill(0), new Array(this.num_groups).fill(0)];
        this.played_ew = [new Array(this.num_groups).fill(0), new Array(this.num_groups).fill(0)];
        this.plays = new Array(4 * this.tricks_total).fill(-1);
        let layout_targets = [];
        for (let i = 0; i < this.dealt_ew.length; ++i)
            if (trick_targets[i])
                layout_targets.push([this.dealt_ew[i], trick_targets[i]]);
        return this.check_p1(layout_targets, 0);
    }
}

class LineSolver
{
    constructor(checker) {
        this.checker = checker;
    }

    expand_single_combination_lines()
    {
        let n = this.checker.dealt_ew.length;
        this.cap = new Array(n).fill(0);
        for (let idx = 0; idx < n; ++idx) {
            let line = new Array(n).fill(0);
            do line[idx]++; while (this.check_line(line))
            this.cap[idx] = line[idx]-1;
        }
    }

    expand_equal_target_lines()
    {
        let n = this.checker.dealt_ew.length;
        let line = new Array(n).fill(0);
        do {
            var expanded = false;
            for (let i = 0; i < n; ++i)
                if (line[i] < this.cap[i]) {
                    expanded = true;
                    ++line[i];
                }
        } while (expanded && this.check_line(line));     
    }

    solve() {
        this.queue = []
        this.fails = []
        this.lines = []
        this.expansion_count = 0;
        this.expand_single_combination_lines();
        this.expand_equal_target_lines();
        this.expand_all_lines();
        return this.lines;
    }

    expand_line_single_index(line, index) {
        if (line[index] == this.cap[index]) return false;
        let new_line = line.slice();
        new_line[index]++;
        for (let prev_line of this.lines)
            if (prev_line.every((tricks, i) => tricks >= new_line[i])) 
                new_line[index] = prev_line[index]+1;
        return (new_line[index] <= this.cap[index]) && this.check_line(new_line);
    }

    dominated(line) {
        for (let prev_line of this.lines)
            if (prev_line.every((tricks, i) => tricks >= line[i])) 
                return true;
        return false;
    }

    expand_line(line) {
        let expanded = false;
        for (let i = 0; i < line.length; ++i)
            expanded |= this.expand_line_single_index(line, i);
        return expanded;
    }

    expand_all_lines() {
        while (this.queue.length) {
            this.queue.sort((a, b) => sum(a) - sum(b));
            let line = this.queue.pop();
            if (!this.expand_line(line) && !this.dominated(line))
                this.lines.push(line);
        }
    }

    check_line_(line) {
        for (let fail of this.fails)
            if (line.every((tricks, i) => tricks >= this.fails[i])) return false;
        if (this.checker.feasible(line)) {
            return true;
        } else {
            this.fails = this.fails.filter(fail => fail.some((tricks, index) => tricks > line[index]));
            this.fails.push(line);
            return false;
        }
    }

    check_line(line) {
        this.expansion_count++;
        rv = this.check_line_(line);
        if (rv) this.queue.push(line.slice(0));
        return rv;
    }
}

class AnalysisResult
{
    constructor(labels, values, formatter, highlight) {
        this.labels = labels;
        this.values = values;
        this.formatter = formatter;
        this.highlight = highlight;
    }

    make_row(table) {
        var row = document.createElement('tr');
        this.labels.forEach(function(label) {
            var cell = document.createElement('th');
            cell.appendChild(document.createTextNode(label));
            row.appendChild(cell);
        });
        let formatter = this.formatter;
        let maxval = Math.max(...this.values);
        let highlight = this.highlight;
        this.values.forEach(function(value) {
            var cell = document.createElement('td');
            cell.appendChild(document.createTextNode(formatter(value)));
            if (highlight && value > maxval - epsilon) cell.style.fontWeight = 'bolder';
            row.appendChild(cell);
        });
        return row;
    }

    filter(keep) {
        return new AnalysisResult(this.labels, this.values.filter((value, index) => keep[index]), this.formatter, this.highlight);
    }
}

function format_percentage(value) {
    return (value * 100).toFixed(2) + "%";
}

function format_2dp(value) {
    return value.toFixed(2);
}

function match_point_scores(sc, lines) {
    score = new Array(lines.length);
    for (let a = 0; a < lines.length; ++a) {
        score[a] = new Array(lines.length);
        for (let b = 0; b < lines.length; ++b) {
            score[a][b] = 0;
            for (let i = 0; i < sc.splits.length; ++i) {
                let this_score = 0;
                if (lines[a][i] > lines[b][i]) this_score = 1;
                if (lines[a][i] < lines[b][i]) this_score = -1;
                score[a][b] += sc.prob[i] * this_score;
            }
        }
    }
    active = new Array(lines.length).fill(true);
    function dominates(a, b) {
        for (let c = 0; c < active.length; ++c)
            if (active[c] && score[a][c] < score[b][c]) return false;
        return true;
    }
    function eliminate_dominated() {
        for (let a = 0; a < active.length; ++a)
            if (active[a])
                for (let b = 0; b < active.length; ++b)
                    if (active[b] && a != b && dominates(a, b)) {
                        active[b] = false;
                        return true;
                    }
        return false;
    }    
    while (eliminate_dominated());
    rv = [];
    for (let a = 0; a < active.length; ++a)
        if (active[a]) {
            scores = [];
            for (let j = 0; j < lines.length; ++j)
                scores.push(score[j][a]*0.5+0.5);
            rv.push(new AnalysisResult(["Match Points"], scores, format_percentage, true));
        }
    return rv;
}

function expected_tricks(sc, lines)
{
    values = [];
    for (let j = 0; j < lines.length; ++j) {
        e = 0.0
        for (let i = 0; i < sc.splits.length; ++i)
            e += sc.prob[i] * lines[j][i];
        values.push(e);
    }
    return new AnalysisResult(["Expected Tricks"], values, format_2dp, true);
}

function guaranteed_tricks(sc, lines, min_tricks, max_tricks)
{
    rv = [];
    for (let tt = min_tricks; tt <= max_tricks; ++tt) {
        if (tt == max_tricks) {
            label = tt + " tricks";
        } else {
            label = tt + "+ tricks";
        }
        minp = 1.0;
        values = [];
        for (let j = 0; j < lines.length; ++j) {
            p = 0.0
            for (let i = 0; i < sc.splits.length; ++i)
                if (lines[j][i] >= tt)
                    p += sc.prob[i];
            if (p < minp) minp = p;
            values.push(p);
        }
        if (minp < 1.0 - epsilon) rv.push(new AnalysisResult([label], values, format_percentage, true));
    }
    return rv;
}

function line_narratives(sc, lc, lines)
{
    values = [];
    for (let j = 0; j < lines.length; ++j) {
        values.push(0);
    }
    cts = (x => sc.group_to_card(x));
    return new AnalysisResult(["First Play"], values, cts, false);
}

function line_descriptions(sc, lines)
{
    rv = [];
    for (let i = 0; i < sc.splits.length; ++i) {
        labels = [sc.split_to_string(sc.splits[i])];
        values = [];
        for (let j = 0; j < lines.length; ++j) {
            values.push(lines[j][i]);
        }
        rv.push(new AnalysisResult(labels, values, x => x, false));
    }
    return rv;
}

function line_label(index)
{
    labels = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
    label = 'Line ';
    do {
        label += labels[index % labels.length];
        index = Math.floor(index / labels.length);
    } while (index);
    return label;
}

function analysis_results(sc, lines, lc, tricks_total, filter)
{
    let labels = new AnalysisResult([""], lines.map((value, index) => line_label(index)), x => x, false);
    let descriptions = line_descriptions(sc, lines);
    let narratives = line_narratives(sc, lc, lines);
    let metrics = [expected_tricks(sc, lines)].concat(match_point_scores(sc, lines), guaranteed_tricks(sc, lines, 0, tricks_total));
    let all_lines = [labels, narratives, ...descriptions, ...metrics];;
    if (!filter) return all_lines;

    let good = Array(lines.length).fill(false);
    for (let i = 0; i < metrics.length; ++i) {
        maxval = Math.max(...metrics[i].values);
        for (let j = 0; j < metrics[i].values.length; ++j) 
            if (metrics[i].values[j] >= maxval - epsilon)
                good[j] = true;
    }

    function dominated(line) {
        for (let other = 0; other < good.length; ++other)
            if (dominates(other, line)) return true;
        return false;
    }

    function dominates(line, other) {
        for (let i = 0; i < metrics.length; ++i)
            if (metrics[i].values[line] <= metrics[i].values[other] - epsilon)
                return false;
        for (let i = 0; i < metrics.length; ++i)
            if (metrics[i].values[line] >= metrics[i].values[other] + epsilon)
                return true;
        return false;
    }

    for (let line = 0; line < good.length; ++line)
        if (dominated(line)) good[line] = false;

    return all_lines.map(ar => ar.filter(good));
}

function analyze(north, south, filter) {
  let sc = new SuitCombination(cards_from_string(north), cards_from_string(south));
  let lc = new LineChecker(sc);
  let ls = new LineSolver(lc);
  let lines = ls.solve();
  let tableData = analysis_results(sc, lines, lc, lc.tricks_total, filter);
  var div_analysis = document.getElementById('analysis');
  div_analysis.innerHTML = '';
  var table = document.createElement('table');
  var tableBody = document.createElement('tbody');
  tableData.forEach(function(row) {
    tableBody.appendChild(row.make_row(table));
  });
  table.appendChild(tableBody);
  div_analysis.appendChild(table);
}

module.exports.cards_from_string = cards_from_string
module.exports.SuitCombination = SuitCombination;
module.exports.LineChecker = LineChecker;
module.exports.LineSolver = LineSolver;
