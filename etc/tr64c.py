"""
@file tr64c.py
@author Daniel Starke
@date 2018-08-15
@version 2018-08-17

DISCLAIMER
This file has no copyright assigned and is placed in the Public Domain.
All contributions are also assumed to be in the Public Domain.
Other contributions are not permitted.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
"""

from datetime import datetime
import json, re, subprocess


def quote(value):
	""" Quotes and escapes the given value to pass it to tr64c """
	esc = {
		'\\': "\\\\",
		'\n': "\\n",
		'\r': "\\r",
		'\t': "\\t",
		'"' : "\\\"",
		'\'': "\\'"
	}
	res = []
	for c in value:
		res.append(esc.get(c, c))
	return '"' + ''.join(res) + '"'
		

def scan(app, localIf, timeout = 1000):
	""" Scan for compliant devices at the given local network interface """
	cmdLine = [app, "-o", localIf, "-t", str(timeout), "--utf8", "-f", "JSON", "-s"]
	ctx = subprocess.Popen(cmdLine, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	res = []
	while True:
		line = ctx.stdout.readline().strip()
		if line == "":
			break
		elif re.match(r"Error:.*", line) != None:
			raise RuntimeError(line)
		res.append(line)
	res = ''.join(res)
	if len(res) > 0:
		return json.loads(res, encoding="UTF-8")
	else:
		return {}


def version(app):
	""" Returns the version of the application """
	cmdLine = [app, "--utf8", "--version"]
	ctx = subprocess.Popen(cmdLine, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	line = ctx.stdout.readline().strip()
	match = re.match(r"(([0-9]+)\.([0-9]+)\.([0-9]+)) ([^ ]+) (.+)", line)
	if match == None:
		return {}
	return {'Version':match.group(1), 'Major':int(match.group(2)), 'Minor':int(match.group(3)), 'Patch':int(match.group(4)), 'Date':datetime.strptime(match.group(5), "%Y-%m-%d"), 'Backend': match.group(6)}


class Session:
	""" TR-064 session instance """
	
	def __init__(self, app, host, timeout = 1000, user = None, password = None, cache = None):
		cmdLine = [app, "-o", host, "-t", str(timeout), "-f", "JSON"]
		if user != None:
			cmdLine.extend(["-u", user])
		if password != None:
			cmdLine.extend(["-p", password])
		if cache != None:
			cmdLine.extend(["-c", cache])
		cmdLine.extend(["--utf8", "-i"])
		self.ctx = subprocess.Popen(cmdLine, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	
	def __del__(self):
		if self.ctx != None:
			self.ctx.terminate()
	
	def list(self):
		""" Returns a list of possible actions and their arguments. """
		self.ctx.stdin.write(b"list\n")
		res = []
		while self.ctx.poll() == None:
			line = self.ctx.stdout.readline().strip()
			if line == "":
				break
			elif re.match(r"Error:.*", line) != None:
				raise RuntimeError(line)
			res.append(line)
		res = ''.join(res)
		if len(res) > 0:
			return json.loads(res, encoding="UTF-8")
		else:
			return {}
	
	def query(self, action, args = []):
		""" Queries an action and returns its result. """
		for i, value in enumerate(args):
			args[i] = quote(value)
		self.ctx.stdin.write(b"query {} {}\n".format(action, ' '.join(args)))
		res = []
		while self.ctx.poll() == None:
			line = self.ctx.stdout.readline().strip()
			if line == "" or re.match(r"Error:.*", line) != None:
				break
			res.append(line);
		res = ''.join(res)
		if len(res) > 0:
			return json.loads(res, encoding="UTF-8").values()[0]
		else:
			return {}


if __name__ == '__main__':
	import sys, os.path
	app = "../bin/tr64c.exe" if os.path.isfile("../bin/tr64c.exe") else "../bin/tr64c"
	ver = version(app)
	print "Using tr64c version {} with backend {}.".format(ver['Version'], ver['Backend'])
	devices = scan(app, "192.168.178.25")
	for dev in devices:
		print "Found {} at {}.".format(dev['Device'], dev['URL'])
	if len(devices) > 0:
		session = Session(app, devices[0]['URL'])
		record = session.list()
		print "Found {} devices in {}.".format(len(record.values()[0]), record.keys()[0])
		hostCount = session.query("Hosts/GetHostNumberOfEntries")['HostNumberOfEntries']
		fmt = "{:17}  {:15}  {}"
		print fmt.format("MAC", "IP", "Host")
		for index in range(1, hostCount):
			record = session.query("Hosts/GetGenericHostEntry", ["HostNumberOfEntries=" + str(index)])
			print fmt.format(record['MACAddress'], record['IPAddress'], record['HostName'])
