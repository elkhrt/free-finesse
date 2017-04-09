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
    return Array.from(partial(0, 1, [], []));
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
            c += sc.groups[g][0] + sc.groups[g][1] + sc.groups[g][2];
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
            c += sc.groups[g][0] + sc.groups[g][1];
        }
        return [e, w];
    }

    split_to_string(split) {
        let cards = this.split_to_cards(split);
        return cards_to_string(cards[1]) + '-' + cards_to_string(cards[0]);
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

    check_play_NS(turn, player, play, continuation) {
        if (this.played_ns[player][play] >= this.dealt_ns[player][play]) return false;
        if (play != -1) this.played_ns[player][play]++;
        this.plays[this.turn++] = play;
        let counter = continuation();
        this.turn--;
        if (play != -1) this.played_ns[player][play]--;
        return !counter;
    }

    is_void_NS(player) {
        for (let play = 0; play < this.num_groups; ++play)
            if (this.played_ns[player][play] < this.dealt_ns[player][play]) return false;
        return true;
    }

    check_play_EW(turn, player, play, continuation) {
        if (play != -1) this.played_ew[player][play]++;
        this.plays[this.turn++] = play;
        let counter = continuation();
        this.turn--;
        if (play != -1) this.played_ew[player][play]--;
        return !counter;
    }

    winning_play_p1(layout_targets) {
        let targets = layout_targets.filter(lt => (lt[1] > 0)
            && (Math.max(sum(lt[0][0]), sum(lt[0][1])) + lt[1] > this.tricks_total));
        if (!targets.length) return true;
        if (targets.some(lt => lt[1] > this.tricks_left)) return false;
        for (let play = 0; play < this.num_groups; ++play) {
            if (this.check_play_NS(0, 0, play, () => this.winning_play_p2_E(targets))) return true;
            if (this.check_play_NS(0, 1, play, () => this.winning_play_p2_W(targets))) return true;
        }
        return false;
    }

    winning_play_EW(turn, player, layouts, continuation) {
        for (let play = 0; play < this.num_groups; ++play) {
            let remaining_layouts = layouts.filter(lt => lt[0][player][play] > this.played_ew[player][play]);
            if (remaining_layouts.length) {
                let next_continuation = (() => continuation(remaining_layouts));
                if (this.check_play_EW(turn, player, play, next_continuation)) return true;
            }
        }
        let void_layouts = layouts.filter(lt => sum(lt[0][player]) <= (this.tricks_total - this.tricks_left));
        if (void_layouts.length) {
            let void_coninutation = (() => continuation(void_layouts));
            return this.check_play_EW(turn, player, -1, void_coninutation);
        }
    }

    winning_play_p2_E(layout_targets) { return this.winning_play_EW(1, 0, layout_targets, (layouts) => this.winning_play_p3_S(layouts)); }
    winning_play_p2_W(layout_targets) { return this.winning_play_EW(1, 1, layout_targets, (layouts) => this.winning_play_p3_N(layouts)); }
    winning_play_p4_E(layout_targets) { return this.winning_play_EW(3, 0, layout_targets, (layouts) => this.winning_play_p5(layouts)); }
    winning_play_p4_W(layout_targets) { return this.winning_play_EW(3, 1, layout_targets, (layouts) => this.winning_play_p5(layouts)); }

    winning_play_p3_NS(player, continuation) {
        if (this.is_void_NS(player)) {
            return this.check_play_NS(2, player, -1, continuation);
        } else {
            for (let play = 0; play < this.num_groups; ++play)
                if (this.check_play_NS(2, player, play, continuation)) return true;
            return false;
        }
    }  

    winning_play_p3_N(layout_targets) { return this.winning_play_p3_NS(0, () => this.winning_play_p4_E(layout_targets)); }
    winning_play_p3_S(layout_targets) { return this.winning_play_p3_NS(1, () => this.winning_play_p4_W(layout_targets)); }

    winning_play_p5(layouts) {
        let i = this.turn - 4;
        let is_ns_trick = (this.plays[i+0] > this.plays[i+1] && this.plays[i+0] > this.plays[i+3])
                        || (this.plays[i+2] > this.plays[i+1] && this.plays[i+2] > this.plays[i+3]);
        let next_layouts = layouts.map(lt => [lt[0], lt[1] - is_ns_trick]);
        this.tricks_left--;
        let wins = this.winning_play_p1(next_layouts);
        this.tricks_left++;
        return wins;
    }

    feasible(trick_targets) {
        this.tricks_left = this.tricks_total;
        this.turn = 0;
        this.played_ns = [new Array(this.num_groups).fill(0), new Array(this.num_groups).fill(0)];
        this.played_ew = [new Array(this.num_groups).fill(0), new Array(this.num_groups).fill(0)];
        this.plays = new Array(4 * this.tricks_left).fill(-1);
        let layout_targets = [];
        for (let i = 0; i < this.dealt_ew.length; ++i)
            if (trick_targets[i])
                layout_targets.push([this.dealt_ew[i], trick_targets[i]]);
        return this.winning_play_p1(layout_targets);
    }

    best_NS_play(trick_targets)
    {
        let layout_targets = [];
        for (let i = 0; i < this.dealt_ew.length; ++i)
            if (trick_targets[i])
                layout_targets.push([this.dealt_ew[i], trick_targets[i]]);
        let targets = layout_targets.filter(lt => (lt[1] > 0)
            && (Math.max(sum(lt[0][0]), sum(lt[0][1])) + lt[1] > this.tricks_total));
        if (targets.some(lt => lt[1] > this.tricks_left)) return -1;
        for (let play = this.num_groups-1; play >= 0; --play) {
            if (this.check_play_NS(0, 0, play, () => this.winning_play_p2_E(targets))) return play;
            if (this.check_play_NS(0, 1, play, () => this.winning_play_p2_W(targets))) return play;
        }
        return -1;
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

sc = new SuitCombination(cards_from_string('AQT98'), cards_from_string('5432'));
sc = new SuitCombination(cards_from_string('J5'), cards_from_string('AQ7642'));
sc = new SuitCombination(cards_from_string('A5'), cards_from_string('JT643'));
sc = new SuitCombination(cards_from_string('AJ8'), cards_from_string('T32'));
sc = new SuitCombination(cards_from_string('AJ8'), cards_from_string('K932'));
sc = new SuitCombination(cards_from_string('AJ4'), cards_from_string('K932'));

lc = new LineChecker(sc);
ls = new LineSolver(lc);
ls.solve();
str = '';
for (let j = 0; j < ls.lines.length; ++j) {
    str += '\t'
    str += "ABCDEFGHIJKL"[j];
}
console.log(str);
str = '';
for (let j = 0; j < ls.lines.length; ++j) {
    str += '\t'
    str += sc.group_to_card([lc.best_NS_play(ls.lines[j])]);
}
console.log(str);
for (let i = 0; i < sc.splits.length; ++i) {
    str = sc.split_to_string(sc.splits[i]);
    for (let j = 0; j < ls.lines.length; ++j) {
        str += '\t'
        str += ls.lines[j][i];
    }
    console.log(str);
}

console.log('Probability of at least the specified number of tricks');
for (let tt = 0; tt <= lc.tricks_total; ++tt) {
    str = '' + tt;
    for (let j = 0; j < ls.lines.length; ++j) {
        str += '\t'
        p = 0.0
        for (let i = 0; i < sc.splits.length; ++i)
            if (ls.lines[j][i] >= tt)
                p += sc.prob[i];
        str += p.toFixed(4);
    }
    console.log(str);
}
console.log('Expected number of tricks');
str = '';
for (let j = 0; j < ls.lines.length; ++j) {
    str += '\t'
    e = 0.0
    for (let i = 0; i < sc.splits.length; ++i)
        e += sc.prob[i] * ls.lines[j][i];
    str += e.toFixed(4);
}
console.log(str);
console.log("Match-point comparisons");
score = new Array(ls.lines.length);
for (let a = 0; a < ls.lines.length; ++a) {
    score[a] = new Array(ls.lines.length);
    for (let b = 0; b < ls.lines.length; ++b) {
        score[a][b] = 0;
        for (let i = 0; i < sc.splits.length; ++i) {
            let this_score = 0;
            if (ls.lines[a][i] > ls.lines[b][i]) this_score = 1;
            if (ls.lines[a][i] < ls.lines[b][i]) this_score = -1;
            score[a][b] += sc.prob[i] * this_score;
        }
    }
}
active = new Array(ls.lines.length).fill(true);
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
while (eliminate_dominated())
{
}

for (let a = 0; a < active.length; ++a)
    if (active[a]) {
        str = "ABCDEFGHIJKL"[a];
        for (let j = 0; j < ls.lines.length; ++j) {
            str += '\t'
            str += (score[j][a]*0.5+0.5).toFixed(4);
        }
        console.log(str);
    }

