"""Read-only baseline/recovery verification through the actual FCD client."""
import importlib.util
import json
from pathlib import Path
import sys

BASE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('fcd_reliability_check',BASE/'FCD.pyw')
m=importlib.util.module_from_spec(spec);sys.modules[spec.name]=m;spec.loader.exec_module(m)
cfg=json.loads((BASE/'fcd_settings.json').read_text(encoding='utf-8'))['cm5']
client=m.Cm5DiagnosticsClient('192.168.1.157',13400,(BASE/'token.txt').read_text().strip(),cfg['fingerprint'])
client.connect()
result={'connected_before':client.connected}
try:
    health=client.request(m.CM5_COMMANDS['current_health'])
    result['health']={'ok':health.get('ok'),'numeric_fields':len(health.get('data',{}).get('numeric',{}))}
except m.Cm5AgentError as exc:
    result['health_error']=str(exc)
result['connected_after_health']=client.connected
if not client.connected: client.connect()
versions=client.request(m.CM5_COMMANDS['versions'])
result['versions_after_health']={'agent':versions['agent_version'],'applications':len(versions['applications'])}
client.disconnect()
print(json.dumps(result,indent=2))
