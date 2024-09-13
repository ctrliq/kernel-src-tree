#!/usr/bin/env python3
import bugzilla

## -------- MAIN

products = ['kernel', 'kernel-rt']

# public instance of bugzilla.redhat.com.
URL = "bugzilla.redhat.com"
bzapi = bugzilla.Bugzilla(URL)
## Log In
if not bzapi.logged_in:
    print("Cached login credentials for %s not found." % URL)
    bzapi.interactive_login()

