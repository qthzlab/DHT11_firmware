"""
DHT11 Arduino SCPI Diagnostic Tool
==================================

Comprehensive diagnostic script to test communication speed,
all commands, and features of the DHT11 Arduino setup.

Author: Abhay's Lab @ GSU
Date: 2025

Usage:
    python DHT11_diagnostic.py
    
Or in Jupyter:
    %run DHT11_diagnostic.py
"""

import pyvisa
import time
import statistics
from datetime import datetime
from typing import List, Tuple, Dict, Optional


# =============================================================================
# CONFIGURATION
# =============================================================================

PORT = 'ASRL3::INSTR'
BAUD_RATE = 115200
TIMEOUT = 5000  # ms

# =============================================================================
# DIAGNOSTIC CLASS
# =============================================================================

class DHT11Diagnostic:
    """Diagnostic tool for DHT11 Arduino setup."""
    
    def __init__(self, port: str = PORT):
        self.port = port
        self.dev = None
        self.results = {}
        
    def connect(self) -> bool:
        """Establish connection to Arduino."""
        print("=" * 60)
        print("DHT11 ARDUINO DIAGNOSTIC TOOL")
        print("=" * 60)
        print(f"\nTimestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print(f"Port: {self.port}")
        print(f"Baud Rate: {BAUD_RATE}")
        print()
        
        try:
            # Close any existing QCoDeS instruments
            try:
                from qcodes.instrument import Instrument
                Instrument.close_all()
                time.sleep(0.5)
            except:
                pass
            
            rm = pyvisa.ResourceManager('@py')
            
            # List available ports
            resources = rm.list_resources()
            print(f"Available ports: {resources}")
            
            if self.port not in resources:
                # Try to find Arduino
                arduino_ports = [r for r in resources if 'ASRL' in r]
                if arduino_ports:
                    self.port = arduino_ports[0]
                    print(f"Using detected port: {self.port}")
                else:
                    print("ERROR: No serial ports found!")
                    return False
            
            print(f"\nConnecting to {self.port}...")
            self.dev = rm.open_resource(self.port)
            self.dev.baud_rate = BAUD_RATE
            self.dev.timeout = TIMEOUT
            
            print("Waiting for Arduino reset (4 seconds)...")
            time.sleep(4)
            
            # Flush buffer
            self._flush()
            
            # Send dummy command to clear garbage
            print("Clearing Arduino input buffer...")
            self.dev.write('X\n')
            time.sleep(0.3)
            self._flush()
            
            print("Connection established!\n")
            return True
            
        except Exception as e:
            print(f"ERROR: Connection failed - {e}")
            return False
    
    def _flush(self) -> int:
        """Flush serial buffer. Returns bytes flushed."""
        old_timeout = self.dev.timeout
        self.dev.timeout = 200
        flushed = 0
        try:
            for _ in range(50):
                try:
                    data = self.dev.read_raw()
                    flushed += len(data)
                except:
                    break
        finally:
            self.dev.timeout = old_timeout
        return flushed
    
    def _query(self, cmd: str, delay: float = 0.3) -> Tuple[str, float]:
        """
        Send command and get response.
        Returns (response, response_time_ms).
        """
        time.sleep(delay)
        
        start = time.perf_counter()
        self.dev.write(cmd + '\n')
        raw = self.dev.read_raw()
        elapsed = (time.perf_counter() - start) * 1000  # ms
        
        response = raw.decode('ascii', errors='ignore').strip()
        return response, elapsed
    
    def disconnect(self):
        """Close connection."""
        if self.dev:
            self.dev.close()
            print("\nConnection closed.")
    
    # =========================================================================
    # DIAGNOSTIC TESTS
    # =========================================================================
    
    def test_identification(self) -> bool:
        """Test *IDN? command."""
        print("-" * 60)
        print("TEST 1: Identification (*IDN?)")
        print("-" * 60)
        
        try:
            response, elapsed = self._query('*IDN?')
            print(f"  Response: {response}")
            print(f"  Time: {elapsed:.1f} ms")
            
            parts = response.split(',')
            if len(parts) >= 4:
                print(f"  Vendor: {parts[0]}")
                print(f"  Model: {parts[1]}")
                print(f"  Serial: {parts[2]}")
                print(f"  Firmware: {parts[3]}")
                print("  Status: PASS ✓")
                self.results['IDN'] = 'PASS'
                return True
            else:
                print("  Status: FAIL ✗ (Invalid response format)")
                self.results['IDN'] = 'FAIL'
                return False
                
        except Exception as e:
            print(f"  Status: FAIL ✗ ({e})")
            self.results['IDN'] = 'FAIL'
            return False
    
    def test_measurement_commands(self) -> bool:
        """Test all measurement commands."""
        print("\n" + "-" * 60)
        print("TEST 2: Measurement Commands")
        print("-" * 60)
        
        all_pass = True
        
        # MEAS:TEMP?
        try:
            response, elapsed = self._query('MEAS:TEMP?')
            temp = float(response)
            print(f"  MEAS:TEMP? = {temp:.2f} °C ({elapsed:.1f} ms) ✓")
            self.results['MEAS:TEMP'] = 'PASS'
        except Exception as e:
            print(f"  MEAS:TEMP? = FAIL ({e}) ✗")
            self.results['MEAS:TEMP'] = 'FAIL'
            all_pass = False
        
        # MEAS:HUM?
        try:
            response, elapsed = self._query('MEAS:HUM?')
            hum = float(response)
            print(f"  MEAS:HUM?  = {hum:.2f} % ({elapsed:.1f} ms) ✓")
            self.results['MEAS:HUM'] = 'PASS'
        except Exception as e:
            print(f"  MEAS:HUM?  = FAIL ({e}) ✗")
            self.results['MEAS:HUM'] = 'FAIL'
            all_pass = False
        
        # MEAS:ALL?
        try:
            response, elapsed = self._query('MEAS:ALL?')
            print(f"  MEAS:ALL?  = {response} ({elapsed:.1f} ms) ✓")
            self.results['MEAS:ALL'] = 'PASS'
        except Exception as e:
            print(f"  MEAS:ALL?  = FAIL ({e}) ✗")
            self.results['MEAS:ALL'] = 'FAIL'
            all_pass = False
        
        return all_pass
    
    def test_configuration_commands(self) -> bool:
        """Test configuration commands."""
        print("\n" + "-" * 60)
        print("TEST 3: Configuration Commands")
        print("-" * 60)
        
        all_pass = True
        
        # CONF:UNIT
        try:
            # Get current
            response, _ = self._query('CONF:UNIT?')
            print(f"  CONF:UNIT? = {response}")
            
            # Set to F
            response, _ = self._query('CONF:UNIT F')
            print(f"  CONF:UNIT F = {response}")
            
            # Verify
            response, _ = self._query('CONF:UNIT?')
            if response == 'F':
                print(f"  Verify = {response} ✓")
            else:
                print(f"  Verify = {response} (expected F) ✗")
                all_pass = False
            
            # Set back to C
            self._query('CONF:UNIT C')
            self.results['CONF:UNIT'] = 'PASS'
            
        except Exception as e:
            print(f"  CONF:UNIT = FAIL ({e}) ✗")
            self.results['CONF:UNIT'] = 'FAIL'
            all_pass = False
        
        # CONF:AVG
        try:
            # Get current
            response, _ = self._query('CONF:AVG?')
            original_avg = int(response)
            print(f"  CONF:AVG?  = {response}")
            
            # Set to 4
            response, _ = self._query('CONF:AVG 4')
            print(f"  CONF:AVG 4 = {response}")
            
            # Verify
            response, _ = self._query('CONF:AVG?')
            if response == '4':
                print(f"  Verify = {response} ✓")
            else:
                print(f"  Verify = {response} (expected 4) ✗")
                all_pass = False
            
            # Restore
            self._query(f'CONF:AVG {original_avg}')
            self.results['CONF:AVG'] = 'PASS'
            
        except Exception as e:
            print(f"  CONF:AVG = FAIL ({e}) ✗")
            self.results['CONF:AVG'] = 'FAIL'
            all_pass = False
        
        return all_pass
    
    def test_system_commands(self) -> bool:
        """Test system commands."""
        print("\n" + "-" * 60)
        print("TEST 4: System Commands")
        print("-" * 60)
        
        all_pass = True
        
        # *OPC?
        try:
            response, elapsed = self._query('*OPC?')
            print(f"  *OPC? = {response} ({elapsed:.1f} ms) ✓")
            self.results['OPC'] = 'PASS'
        except Exception as e:
            print(f"  *OPC? = FAIL ({e}) ✗")
            self.results['OPC'] = 'FAIL'
            all_pass = False
        
        # SYST:ERR?
        try:
            response, elapsed = self._query('SYST:ERR?')
            print(f"  SYST:ERR? = {response} ({elapsed:.1f} ms) ✓")
            self.results['SYST:ERR'] = 'PASS'
        except Exception as e:
            print(f"  SYST:ERR? = FAIL ({e}) ✗")
            self.results['SYST:ERR'] = 'FAIL'
            all_pass = False
        
        # SYST:MODE?
        try:
            response, elapsed = self._query('SYST:MODE?')
            print(f"  SYST:MODE? = {response} ({elapsed:.1f} ms) ✓")
            self.results['SYST:MODE'] = 'PASS'
        except Exception as e:
            print(f"  SYST:MODE? = FAIL ({e}) ✗")
            self.results['SYST:MODE'] = 'FAIL'
            all_pass = False
        
        # SYST:INTV?
        try:
            response, elapsed = self._query('SYST:INTV?')
            print(f"  SYST:INTV? = {response} ms ({elapsed:.1f} ms) ✓")
            self.results['SYST:INTV'] = 'PASS'
        except Exception as e:
            print(f"  SYST:INTV? = FAIL ({e}) ✗")
            self.results['SYST:INTV'] = 'FAIL'
            all_pass = False
        
        return all_pass
    
    def test_error_handling(self) -> bool:
        """Test error handling."""
        print("\n" + "-" * 60)
        print("TEST 5: Error Handling")
        print("-" * 60)
        
        all_pass = True
        
        # Invalid command
        try:
            response, _ = self._query('INVALID_COMMAND')
            if 'ERR:100' in response:
                print(f"  Invalid command → {response} ✓")
                self.results['ERR_HANDLING'] = 'PASS'
            else:
                print(f"  Invalid command → {response} (expected ERR:100) ✗")
                self.results['ERR_HANDLING'] = 'FAIL'
                all_pass = False
        except Exception as e:
            print(f"  Error handling = FAIL ({e}) ✗")
            self.results['ERR_HANDLING'] = 'FAIL'
            all_pass = False
        
        # Invalid parameter
        try:
            response, _ = self._query('CONF:AVG 99')
            if 'ERR:102' in response:
                print(f"  Invalid param → {response} ✓")
            else:
                print(f"  Invalid param → {response}")
        except:
            pass
        
        return all_pass
    
    def test_communication_speed(self, num_samples: int = 20) -> Dict:
        """Test communication speed and reliability."""
        print("\n" + "-" * 60)
        print(f"TEST 6: Communication Speed ({num_samples} samples)")
        print("-" * 60)
        
        results = {
            'MEAS:TEMP?': [],
            'MEAS:HUM?': [],
            'MEAS:ALL?': [],
            '*IDN?': []
        }
        
        errors = 0
        
        print(f"  Running {num_samples} iterations...")
        
        for i in range(num_samples):
            for cmd in results.keys():
                try:
                    _, elapsed = self._query(cmd, delay=0.1)
                    results[cmd].append(elapsed)
                except Exception as e:
                    errors += 1
        
        print(f"\n  Results:")
        print(f"  {'Command':<15} {'Min':>8} {'Max':>8} {'Mean':>8} {'StdDev':>8}")
        print(f"  {'-'*15} {'-'*8} {'-'*8} {'-'*8} {'-'*8}")
        
        for cmd, times in results.items():
            if times:
                min_t = min(times)
                max_t = max(times)
                mean_t = statistics.mean(times)
                std_t = statistics.stdev(times) if len(times) > 1 else 0
                print(f"  {cmd:<15} {min_t:>7.1f}ms {max_t:>7.1f}ms {mean_t:>7.1f}ms {std_t:>7.1f}ms")
        
        total_queries = num_samples * len(results)
        success_rate = ((total_queries - errors) / total_queries) * 100
        
        print(f"\n  Total queries: {total_queries}")
        print(f"  Errors: {errors}")
        print(f"  Success rate: {success_rate:.1f}%")
        
        self.results['SPEED_TEST'] = f"{success_rate:.1f}% success"
        
        return results
    
    def test_measurement_stability(self, num_samples: int = 10) -> Dict:
        """Test measurement stability over multiple readings."""
        print("\n" + "-" * 60)
        print(f"TEST 7: Measurement Stability ({num_samples} samples)")
        print("-" * 60)
        
        temps = []
        hums = []
        
        print(f"  Collecting {num_samples} measurements (3s intervals)...")
        print()
        
        for i in range(num_samples):
            try:
                response, _ = self._query('MEAS:ALL?')
                parts = response.replace('TEMP:', '').replace('HUM:', '').split(',')
                temp = float(parts[0])
                hum = float(parts[1])
                temps.append(temp)
                hums.append(hum)
                print(f"  [{i+1:2d}/{num_samples}] T = {temp:.2f}°C, H = {hum:.2f}%")
                
                if i < num_samples - 1:
                    time.sleep(3)  # DHT11 needs ~2s between readings
                    
            except Exception as e:
                print(f"  [{i+1:2d}/{num_samples}] ERROR: {e}")
        
        if temps:
            print(f"\n  Temperature Statistics:")
            print(f"    Mean: {statistics.mean(temps):.2f} °C")
            print(f"    StdDev: {statistics.stdev(temps) if len(temps) > 1 else 0:.2f} °C")
            print(f"    Min: {min(temps):.2f} °C")
            print(f"    Max: {max(temps):.2f} °C")
            
            print(f"\n  Humidity Statistics:")
            print(f"    Mean: {statistics.mean(hums):.2f} %")
            print(f"    StdDev: {statistics.stdev(hums) if len(hums) > 1 else 0:.2f} %")
            print(f"    Min: {min(hums):.2f} %")
            print(f"    Max: {max(hums):.2f} %")
        
        return {'temperature': temps, 'humidity': hums}
    
    def test_reset(self) -> bool:
        """Test reset command."""
        print("\n" + "-" * 60)
        print("TEST 8: Reset Command")
        print("-" * 60)
        
        try:
            # Change a setting
            self._query('CONF:AVG 8')
            
            # Reset
            response, elapsed = self._query('*RST')
            print(f"  *RST = {response} ({elapsed:.1f} ms)")
            
            # Verify reset
            response, _ = self._query('CONF:AVG?')
            if response == '1':  # Default value
                print(f"  Verify AVG reset = {response} ✓")
                self.results['RESET'] = 'PASS'
                return True
            else:
                print(f"  Verify AVG reset = {response} (expected 1) ✗")
                self.results['RESET'] = 'FAIL'
                return False
                
        except Exception as e:
            print(f"  Reset = FAIL ({e}) ✗")
            self.results['RESET'] = 'FAIL'
            return False
    
    def print_summary(self):
        """Print test summary."""
        print("\n" + "=" * 60)
        print("DIAGNOSTIC SUMMARY")
        print("=" * 60)
        
        passed = sum(1 for v in self.results.values() if 'PASS' in str(v))
        total = len(self.results)
        
        print(f"\n  {'Test':<20} {'Result':<15}")
        print(f"  {'-'*20} {'-'*15}")
        
        for test, result in self.results.items():
            status = "✓" if 'PASS' in str(result) else "✗"
            print(f"  {test:<20} {result:<15} {status}")
        
        print(f"\n  Total: {passed}/{total} tests passed")
        
        if passed == total:
            print("\n  ★ ALL TESTS PASSED ★")
        else:
            print(f"\n  ⚠ {total - passed} test(s) failed")
        
        print()
    
    def run_all_tests(self, include_stability: bool = False):
        """Run all diagnostic tests."""
        if not self.connect():
            return
        
        try:
            self.test_identification()
            self.test_measurement_commands()
            self.test_configuration_commands()
            self.test_system_commands()
            self.test_error_handling()
            self.test_communication_speed(num_samples=10)
            
            if include_stability:
                self.test_measurement_stability(num_samples=10)
            
            self.test_reset()
            self.print_summary()
            
        finally:
            self.disconnect()


# =============================================================================
# MAIN
# =============================================================================

if __name__ == '__main__':
    print("\n")
    
    # Run diagnostics
    diag = DHT11Diagnostic()
    
    # Run all tests (set include_stability=True for longer stability test)
    diag.run_all_tests(include_stability=False)
    
    print("\nTo run with stability test:")
    print("  diag = DHT11Diagnostic()")
    print("  diag.run_all_tests(include_stability=True)")