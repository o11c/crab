import sys


# This class uses `t` instead of `self` to match the C version.
class Table:
    def __init__(t, out=None):
        if out is None:
            out = sys.stdout
        t.out = out
        t._phase = 0
        t.inhibitions = 0

        t.horiz = None
        t.vert = None
        t.cross = None
        t.pad = None
        t.drawing("═", " │ ", "═╪═", " ");

        t.col_widths = []
        t.log_col = 0
        t.tw = 0
        t.softspace = 0

    @property
    def ncols(t):
        return len(t.col_widths)

    def drawing(t, horiz, vert, cross, pad):
        if horiz is not None:
            t.horiz = horiz
        if vert is not None:
            t.vert = vert
        if cross is not None:
            t.cross = cross
        if pad is not None:
            t.pad = pad

    def phase(t):
        '''
            Phase 0: before the loop has begun - maybe add some setup options here?
            Phase 1: go through the loop and record all the sizes
            Phase 2: go through the loop and actually emit the cells
            Phase 3: after the loop has ended ... and table has been freed!
        '''
        assert 0 <= t._phase <= 2
        assert t.log_col == 0
        t._phase += 1
        if t._phase == 3:
            # calls free() in C version
            t.col_widths = None
            t = None
            return False
        return True

    def divider_row(t):
        if t._phase == 1:
            return
        for i in range(t.ncols):
            if i:
                t.out.write(t.cross)
            for j in range(t.col_widths[i]):
                t.out.write(t.horiz)
        t.out.write('\n')

    def end_row(t):
        t.log_col = 0
        t.tw = 0
        t.softspace = 0
        if t._phase == 2:
            t.out.write('\n')

    def hold(t):
        '''
            If you write:

            t.emits("foo");
            t.hold(1);
            t.emits("bar");
            t.emits("baz");
            t.emits("qux");
            t.end_row();

            ... then the output will be:

            foo | barbaz | qux
        '''

    def emit(t, s):
        # TODO wcwidth
        s = str(s)
        l = len(s)

        if t.log_col == t.ncols:
            t.col_widths.append(0)
        assert t.log_col < t.ncols

        if t._phase == 2:
            if t.softspace:
                t.out.write(t.pad * (t.softspace - 1))
                t.softspace = 0
                t.out.write(t.vert)
            t.out.write(s)

        t.tw += l
        if t.tw > t.col_widths[t.log_col]:
            t.col_widths[t.log_col] = t.tw

        t.inhibitions -= 1
        if t.inhibitions < 0:
            t.softspace = 1 + t.col_widths[t.log_col] - t.tw
            t.tw = 0
            t.log_col += 1
            t.inhibitions = 0
