const crypto = require('crypto');
const uid = '04:39:20:02:E5:75:80';
const mac = '0C:9D:92:87:CB:6A';
const target = '070VSDMp';

const ub = Buffer.from(uid.replace(/:/g, ''), 'hex');
const mb = Buffer.from(mac.replace(/:/g, ''), 'hex');

function go(k, m, kn, mn) {
  const h = crypto.createHmac('sha256', k).update(m).digest();
  const b64url = h.toString('base64url');
  const b64 = h.toString('base64');
  const match8 = b64url.slice(-8) === target || b64url.slice(0, 8) === target;
  const match6 = h.slice(-6).toString('base64url') === target || h.slice(0, 6).toString('base64url') === target;
  const tag = (match8 || match6) ? ' *** MATCH ***' : '';
  console.log(kn + '=>' + mn + tag);
  console.log('  b64url: ' + b64url);
  console.log('  first8: ' + b64url.slice(0, 8) + '  last8: ' + b64url.slice(-8));
  console.log('  first6_b64url: ' + h.slice(0, 6).toString('base64url') + '  last6_b64url: ' + h.slice(-6).toString('base64url'));
}

go(mb, ub, 'mac_bytes', 'uid_bytes');
go(ub, mb, 'uid_bytes', 'mac_bytes');
go(Buffer.from(mac), Buffer.from(uid), 'mac_str', 'uid_str');
go(Buffer.from(uid), Buffer.from(mac), 'uid_str', 'mac_str');
go(Buffer.from(mac.toLowerCase()), Buffer.from(uid.toLowerCase()), 'mac_str_low', 'uid_str_low');
go(Buffer.from(uid.toLowerCase()), Buffer.from(mac.toLowerCase()), 'uid_str_low', 'mac_str_low');
go(Buffer.from(mac.replace(/:/g, '')), Buffer.from(uid.replace(/:/g, '')), 'mac_hex_up_str', 'uid_hex_up_str');
go(Buffer.from(uid.replace(/:/g, '')), Buffer.from(mac.replace(/:/g, '')), 'uid_hex_up_str', 'mac_hex_up_str');
go(Buffer.from(mac.replace(/:/g, '').toLowerCase()), Buffer.from(uid.replace(/:/g, '').toLowerCase()), 'mac_hex_low_str', 'uid_hex_low_str');
go(Buffer.from(uid.replace(/:/g, '').toLowerCase()), Buffer.from(mac.replace(/:/g, '').toLowerCase()), 'uid_hex_low_str', 'mac_hex_low_str');

// Also try SHA-1
function goSha1(k, m, kn, mn) {
  const h = crypto.createHmac('sha1', k).update(m).digest();
  const b64url = h.toString('base64url');
  const match = b64url.includes(target) || h.slice(-6).toString('base64url') === target || h.slice(0, 6).toString('base64url') === target;
  if (match) console.log('SHA1 MATCH: ' + kn + '=>' + mn + ' b64url=' + b64url);
}
goSha1(mb, ub, 'mac_bytes', 'uid_bytes');
goSha1(ub, mb, 'uid_bytes', 'mac_bytes');
goSha1(Buffer.from(mac), Buffer.from(uid), 'mac_str', 'uid_str');
goSha1(Buffer.from(uid), Buffer.from(mac), 'uid_str', 'mac_str');

console.log('\nTarget:', target);
