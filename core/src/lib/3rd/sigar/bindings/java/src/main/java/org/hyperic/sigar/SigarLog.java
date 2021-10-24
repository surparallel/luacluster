/*
 * Copyright (c) 2006 Hyperic, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.hyperic.sigar;


import org.apache.logging.log4j.Level;
import org.apache.logging.log4j.LogManager;
import org.apache.logging.log4j.Logger;

import static org.apache.logging.log4j.Level.*;

public class SigarLog {

    //from sigar_log.h
    private static final int LOG_FATAL  = 0;
    private static final int LOG_ERROR  = 1;
    private static final int LOG_WARN   = 2;
    private static final int LOG_INFO   = 3;
    private static final int LOG_DEBUG  = 4;

    private static final boolean enableLogFallbackConf = ! Boolean.getBoolean("sigar.noLog4jDefaultConfig");

    private static native void setLogger(Sigar sigar, Logger log);

    public static native void setLevel(Sigar sigar, int level);

    private static Logger getLogger() {
        return getLogger("Sigar");
    }

    public static Logger getLogger(String name) {
        return LogManager.getLogger(name);
    }

    static void error(String msg, Throwable exc) {
        getLogger().error(msg, exc);
    }

    static void debug(String msg, Throwable exc) {
        getLogger().debug(msg, exc);
    }

    //XXX want to make this automatic, but also dont always
    //want to turn on logging, since most sigar logging will be DEBUG
    public static void enable(Sigar sigar) {
        Logger log = getLogger();

        Level level = log.getLevel();
        if (level == null) {
            level = LogManager.getRootLogger().getLevel();
            if (level == null) {
                return;
            }
        }

        if(level == Level.INFO) {
            setLevel(sigar, LOG_INFO);
        } else if(level == Level.WARN) {
            setLevel(sigar, LOG_WARN);
        } else if (level == Level.ERROR) {
            setLevel(sigar, LOG_ERROR);
        } else if (level == Level.FATAL) {
            setLevel(sigar, LOG_FATAL);
        } else if (level == Level.DEBUG) {
            setLevel(sigar, LOG_DEBUG);
        }

        setLogger(sigar, log);
    }

    public static void disable(Sigar sigar) {
        setLogger(sigar, null);
    }
}
