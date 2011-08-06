import smtplib

from email.mime.text import MIMEText

import web

from EMPMaster import CONFIG
from EMPMaster.Errors import SMTPError

_REGISTRATION_TEMPLATE = """\
We've received your registration request, and once you verify your registration
you will be able to use the master server "%s".   Please visit the following
URL to verify your registration: %s://%s/registration/%d.  Note that
registration tokens are only valid for a short time, after which you will be
required to re-register."""

_TOKENS_EXHAUSTED_TEMPLATE = """\
It appears that your master server has no more registration tokens to assign to
new users.  Most likely this is the result of your server being the victim of a
DoS attack; it is advisable to check your web server's logs and take
take appropriate action (firewall their IP address, contact their ISP, etc.).
Otherwise you may have just been (extremely) unlucky.  If you are getting these
messages often and do not seem to be under attack, contact the developers and
ask for help.  Be prepared to send server logs and database SQL dumps (with
user passwords manually set to dummy values by you)."""

_BAD_JSON_TEMPLATE = """\
Server "%s" sent invalid JSON.\nError: %s.\nData:\n%s"""

###
# [CG] I know web.py has web.sendmail(), but it apparently doesn't work.
###

def send_email(recipients, subject, body):
    msg = MIMEText(body)
    msg['Subject'] = subject
    msg['From'] = CONFIG['smtp_sender']
    msg['To'] = ', '.join(recipients)
    server = smtplib.SMTP(CONFIG['smtp_address'], CONFIG['smtp_port'])
    try:
        code, response = server.ehlo()
        if code != 250:
            raise SMTPError('Error during EHLO', code, response)
        if CONFIG['smtp_use_encryption']:
            code, response = server.starttls()
            if code != 220:
                raise SMTPError('Error during auth exchange', code, response)
            code, response = server.ehlo()
            if code != 250:
                raise SMTPError('Error during encrypted EHLO', code, response)
        code, response = server.login(
            CONFIG['smtp_username'],
            CONFIG['smtp_password']
        )
        if code != 235:
            raise SMTPError('Error during login', code, response)
        server.sendmail(CONFIG['smtp_sender'], recipients, msg.as_string())
    except SMTPError:
        raise
    except Exception, e:
        raise SMTPError('Error during sendmail', -1, str(e))
    finally:
        server.quit()

def admin_message(subject, body):
    send_email([CONFIG['admin_email']], subject, body)

def send_registration(recipient, token):
    body = _REGISTRATION_TEMPLATE % (
        CONFIG['server_name'], web.ctx.protocol, web.ctx.host, token
    )
    subject = 'Verify registration on "%s"' % (CONFIG['server_name'])
    send_email([recipient], subject, body)

def send_tokens_exhausted():
    admin_message(
        'Registration tokens exhausted on "%s"' % (CONFIG['server_name']),
        _TOKENS_EXHAUSTED_TEMPLATE
    )

def send_bad_json(server_name, error, data):
    admin_message(
        'Bad JSON error on "%s"' % (CONFIG['server_name']),
        _BAD_JSON_TEMPLATE % (server_name, error, data)
    )

