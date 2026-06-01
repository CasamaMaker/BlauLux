// ── Sistema de debug estructurat ────────────────────────────
const DBG = {
  _s: {
    CONFIG: 'color:#2980b9;font-weight:700',
    ACTION: 'color:#27ae60;font-weight:700',
    CHANGE: 'color:#e67e22;font-weight:700',
    VALID:  'color:#e74c3c;font-weight:700',
    HW:     'color:#8e44ad;font-weight:700',
    OK:     'color:#1abc9c;font-weight:700',
  },
  _ts() { return new Date().toTimeString().slice(0, 8); },
  _fmt(tag, msg) { return `%c[${this._ts()}][${tag}] ${msg}`; },
  config(msg, data) {
    data !== undefined
      ? console.log(this._fmt('CONFIG', msg), this._s.CONFIG, data)
      : console.log(this._fmt('CONFIG', msg), this._s.CONFIG);
  },
  action(msg, data) {
    data !== undefined
      ? console.log(this._fmt('ACTION', msg), this._s.ACTION, data)
      : console.log(this._fmt('ACTION', msg), this._s.ACTION);
  },
  change(msg, data) {
    data !== undefined
      ? console.log(this._fmt('CHANGE', msg), this._s.CHANGE, data)
      : console.log(this._fmt('CHANGE', msg), this._s.CHANGE);
  },
  valid(msg, data) {
    data !== undefined
      ? console.warn(this._fmt('VALID', msg), this._s.VALID, data)
      : console.warn(this._fmt('VALID', msg), this._s.VALID);
  },
  hw(msg, data) {
    data !== undefined
      ? console.log(this._fmt('HW', msg), this._s.HW, data)
      : console.log(this._fmt('HW', msg), this._s.HW);
  },
  ok(msg, data) {
    data !== undefined
      ? console.log(this._fmt('OK', msg), this._s.OK, data)
      : console.log(this._fmt('OK', msg), this._s.OK);
  },
};
