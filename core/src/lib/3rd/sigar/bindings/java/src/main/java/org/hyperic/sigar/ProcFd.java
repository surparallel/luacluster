/*****************************************************
 * WARNING: this file was generated by -e
 * on Wed Feb 15 18:26:49 2017.
 * Any changes made here will be LOST.
 *****************************************************/
package org.hyperic.sigar;

import java.util.HashMap;
import java.util.Map;

/**
 * ProcFd sigar class.
 */
public class ProcFd implements java.io.Serializable {

    private static final long serialVersionUID = 948L;

    public ProcFd() { }

    public native void gather(Sigar sigar, long pid) throws SigarException;

    /**
     * This method is not intended to be called directly.
     * use Sigar.getProcFd() instead.
     * @exception SigarException on failure.
     * @see org.hyperic.sigar.Sigar#getProcFd
     */
    static ProcFd fetch(Sigar sigar, long pid) throws SigarException {
        ProcFd procFd = new ProcFd();
        procFd.gather(sigar, pid);
        return procFd;
    }

    long total = 0;

    /**
     * Get the Total number of open file descriptors.<p>
     * Supported Platforms: AIX, HPUX, Linux, Solaris, Win32.
     * <p>
     * System equivalent commands:<ul>
     * <li> AIX: <code>lsof</code><br>
     * <li> Darwin: <code>lsof</code><br>
     * <li> FreeBSD: <code>lsof</code><br>
     * <li> HPUX: <code>lsof</code><br>
     * <li> Linux: <code>lsof</code><br>
     * <li> Solaris: <code>lsof</code><br>
     * <li> Win32: <code></code><br>
     * </ul>
     * @return Total number of open file descriptors
     */
    public long getTotal() { return total; }

    void copyTo(ProcFd copy) {
        copy.total = this.total;
    }

    public Map toMap() {
        Map map = new HashMap();
        String strtotal = 
            String.valueOf(this.total);
        if (!"-1".equals(strtotal))
            map.put("Total", strtotal);
        return map;
    }

    public String toString() {
        return toMap().toString();
    }

}
