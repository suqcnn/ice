// **********************************************************************
//
// Copyright(c) 2003-2009 ZeroC, Inc. All rights reserved.
//
// This copy of Ice is licensed to you under the terms described in the
// ICE_LICENSE file included in this distribution.
//
// **********************************************************************

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;
using System.Diagnostics;

namespace Ice.wpf.client
{
    /// <summary>
    /// Interaction logic for HelloWindow.xaml
    /// </summary>
    public partial class HelloWindow : Window
    {
        public HelloWindow()
        {
            InitializeComponent();
            locateOnScreen(this);
        }

        static String TWOWAY = "Twoway";
        static String TWOWAY_SECURE = "Twoway Secure";
        static String ONEWAY = "Oneway";
        static String ONEWAY_BATCH = "Oneway Batch";
        static String ONEWAY_SECURE = "Oneway Secure";
        static String ONEWAY_SECURE_BATCH = "Oneway Secure Batch";
        static String DATAGRAM = "Datagram";
        static String DATAGRAM_BATCH = "Datagram Batch";

        private void Window_Loaded(object sender, EventArgs e)
        {
            try
            {
                Ice.InitializationData initData = new Ice.InitializationData();
                initData.properties = Ice.Util.createProperties();
                initData.properties.load("config.client");
                _communicator = Ice.Util.initialize(initData);
            }
            catch(Ice.LocalException ex)
            {
                handleException(ex);
            }
        }

        private void Window_Closed(object sender, EventArgs e)
        {
            if(_communicator == null)
            {
                return;
            }

            _communicator.destroy();
            _communicator = null;
        }

        private bool deliveryModeIsBatch()
        {
            return deliveryMode.Text.Equals(ONEWAY_BATCH) ||
                    deliveryMode.Text.Equals(ONEWAY_SECURE_BATCH) ||
                    deliveryMode.Text.Equals(DATAGRAM_BATCH);
        }

        private Ice.ObjectPrx deliveryModeApply(Ice.ObjectPrx prx)
        {
            if(deliveryMode.Text.Equals(TWOWAY))
            {
                prx = prx.ice_twoway();
            }
            else if(deliveryMode.Text.Equals(TWOWAY_SECURE))
            {
                prx = prx.ice_twoway().ice_secure(true);
            }
            else if(deliveryMode.Text.Equals(ONEWAY))
            {
                prx = prx.ice_oneway();
            }
            else if(deliveryMode.Text.Equals(ONEWAY_BATCH))
            {
                prx = prx.ice_batchOneway();
            }
            else if(deliveryMode.Text.Equals(ONEWAY_SECURE))
            {
                prx = prx.ice_oneway().ice_secure(true);
            }
            else if(deliveryMode.Text.Equals(ONEWAY_SECURE_BATCH))
            {
                prx = prx.ice_batchOneway().ice_secure(true);
            }
            else if(deliveryMode.Text.Equals(DATAGRAM))
            {
                prx = prx.ice_datagram();
            }
            else if(deliveryMode.Text.Equals(DATAGRAM_BATCH))
            {
                prx = prx.ice_batchDatagram();
            }

            return prx;
        }

        class SayHelloI : Demo.AMI_Hello_sayHello, Ice.AMISentCallback
        {
            public SayHelloI(HelloWindow window)
            {
                _window = window;
            }
            
            public override void ice_response()
            {
                lock(this)
                {
                    Debug.Assert(!_response);
                    _response = true;
                    _window.Dispatcher.BeginInvoke(DispatcherPriority.Normal,(Action)delegate()
                    {
                        _window.status.Content = "Ready";
                    });
                }
            }

            public override void ice_exception(Exception ex)
            {
                lock(this)
                {
                    Debug.Assert(!_response);
                    _response = true;
                    _window.Dispatcher.BeginInvoke(DispatcherPriority.Normal,(Action)delegate()
                    {
                        _window.handleException(ex);
                    });
                }
            }

            #region AMISentCallback Members

            public void ice_sent()
            {
                lock(this)
                {
                    if(_response)
                    {
                        return;
                    }
                    _window.Dispatcher.BeginInvoke(DispatcherPriority.Normal,(Action)delegate()
                    {
                        if(_window.deliveryMode.Text.Equals(TWOWAY) || _window.deliveryMode.Text.Equals(TWOWAY_SECURE))
                        {
                            _window.status.Content = "Waiting for response";
                        }
                        else
                        {
                            _window.status.Content = "Ready";
                        }
                    });
                }
            }

            #endregion

            private bool _response = false;
            private HelloWindow _window;
        }

        private void sayHello_Click(object sender, RoutedEventArgs e)
        {
            Demo.HelloPrx hello = createProxy();
            if(hello == null)
            {
                return;
            }

            int delay =(int)delaySlider.Value;
            try
            {
                if(!deliveryModeIsBatch())
                {
                    if(hello.sayHello_async(new SayHelloI(this), delay))
                    {
                        if(deliveryMode.Text.Equals(TWOWAY) || deliveryMode.Text.Equals(TWOWAY_SECURE))                        
                        {
                            status.Content = "Waiting for response";
                        }
                    }
                    else
                    {
                        status.Content = "Sending request";
                    }
                }
                else
                {
                    flush.IsEnabled = true;
                    hello.sayHello(delay);
                    status.Content = "Queued sayHello request";
                }
            }
            catch(Ice.LocalException ex)
            {
                handleException(ex);
            }
        }

        private void handleException(Exception ex)
        {
            this.Dispatcher.BeginInvoke(DispatcherPriority.Normal,(Action)delegate()
            {
                status.Content = ex.GetType();
            });
        }

        class ShutdownI : Demo.AMI_Hello_shutdown
        {
            public ShutdownI(HelloWindow window)
            {
                _window = window;
            }
            
            public override void ice_response()
            {
                lock(this)
                {
                    _window.Dispatcher.BeginInvoke(DispatcherPriority.Normal,(Action)delegate()
                    {
                        _window.status.Content = "Ready";
                    });
                }
            }

            public override void ice_exception(Exception ex)
            {
                lock(this)
                {
                    _window.Dispatcher.BeginInvoke(DispatcherPriority.Normal,(Action)delegate()
                    {
                        _window.handleException(ex);
                    });
                }
            }

            private HelloWindow _window;
        }

        private void shutdown_Click(object sender, RoutedEventArgs e)
        {
            Demo.HelloPrx hello = createProxy();
            if(hello == null)
            {
                return;
            }

            int delay =(int)delaySlider.Value;

            try
            {
                if(!deliveryModeIsBatch())
                {
                    if(hello.shutdown_async(new ShutdownI(this)))
                    {
                        if(deliveryMode.Text.Equals("Twoway") || deliveryMode.Text.Equals("Twoway Secure"))
                        {
                            status.Content = "Waiting for response";
                        }
                    }
                    else
                    {
                        status.Content = "Sending request";
                    }
                }
                else
                {
                    flush.IsEnabled = true;
                    hello.shutdown();
                    status.Content = "Queued shutdown request";
                }
            }
            catch(Ice.LocalException ex)
            {
                handleException(ex);
            }
        }

        private void flush_Click(object sender, RoutedEventArgs e)
        {
            new System.Threading.Thread(new System.Threading.ThreadStart(delegate()
                {
                    try
                    {
                        _communicator.flushBatchRequests();
                    }
                    catch(Ice.LocalException ex)
                    {
                        this.Dispatcher.BeginInvoke(DispatcherPriority.Normal,(Action)delegate()
                        {
                            handleException(ex);
                        });
                    }
                })).Start();
            flush.IsEnabled = false;
            status.Content = "Flushed batch requests";
        }

        Demo.HelloPrx
        createProxy()
        {
            String host = hostname.Text.Trim();
            if(host.Length == 0)
            {
                status.Content = "No hostname";
                return null;
            }

            String s = "hello:tcp -h " + host + " -p 10000:ssl -h " + host + " -p 10001:udp -h " + host + " -p 10000";
            Ice.ObjectPrx prx = _communicator.stringToProxy(s);
            prx = deliveryModeApply(prx);
            int timeout =(int)timeoutSlider.Value;
            if(timeout != 0)
            {
                prx = prx.ice_timeout(timeout);
            }
            return Demo.HelloPrxHelper.uncheckedCast(prx);
        }

        private void timeoutSlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            timeoutLabel.Content =(timeoutSlider.Value / 1000.0).ToString("F1");
        }

        private void delaySlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            delayLabel.Content =(delaySlider.Value / 1000.0).ToString("F1");
        }

        static private void locateOnScreen(System.Windows.Window window)
        {
            window.Left =(System.Windows.SystemParameters.PrimaryScreenWidth - window.Width) / 2;
            window.Top =(System.Windows.SystemParameters.PrimaryScreenHeight - window.Height) / 2;
        }

        private Ice.Communicator _communicator = null;
    }
}