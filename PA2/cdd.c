#include <linux/init.h>		// Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>	// Core header for loading LKMs into the kernel
#include <linux/device.h>	// Header to support the kernel Driver Model
#include <linux/kernel.h>	// Contains types, macros, functions for the kernel
#include <linux/fs.h>		// Header for the Linux file system support
#include <asm/uaccess.h>	// Required for the copy to user function

#define  DEVICE_NAME "cdd"	///< The device will appear at /dev/ebbchar using this value
#define  CLASS_NAME  "cdd"	///< The device class -- this is a character device driver

//Basic Queue Start

#define MAX 2000

static char bufferD[MAX];
static int frontP = 0;
static int backP = -1;
static int count = 0;

static bool
isEmpty (void)
{
  return count == 0;
}

static bool
isFull (void)
{
  return count == MAX;
}

static int
add (char data)
{

  if (!isFull ())
    {

      if (backP == MAX - 1)
	{
	  backP = -1;
	}

      bufferD[++backP] = data;
      count++;
      return 1;
    }
  else
    {
      return 0;
    }
}

static char
get (void)
{
  char data = bufferD[frontP++];

  if (frontP == MAX)
    {
      frontP = 0;
    }

  count--;
  return data;
}

//END QUEUE

MODULE_LICENSE ("GPL");		///< The license type
MODULE_AUTHOR ("Mark McCulloh, Christopher Williams, Kevin Shoults");	///< modinfo
MODULE_DESCRIPTION ("simple Linux char driver");	///< modinfo
MODULE_VERSION ("0.1");		///< modinfo

static int majorNumber;		///< Stores the device number
static struct class *cddClass = NULL;	///< The device-driver class struct pointer
static struct device *cddDevice = NULL;	///< The device-driver device struct pointer

// The prototype functions for the character driver -- must come before the struct definition
static int dev_open (struct inode *, struct file *);
static int dev_release (struct inode *, struct file *);
static ssize_t dev_read (struct file *, char *, size_t, loff_t *);
static ssize_t dev_write (struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops = {
  .open = dev_open,
  .read = dev_read,
  .write = dev_write,
  .release = dev_release,
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init
cdd_init (void)
{
  printk (KERN_INFO "CDD: Initializing the CDD LKM\n");

  // Try to dynamically allocate a major number for the device -- more difficult but worth it
  majorNumber = register_chrdev (0, DEVICE_NAME, &fops);
  if (majorNumber < 0)
    {
      printk (KERN_ALERT "CDD failed to register a major number\n");
      return majorNumber;
    }
  printk (KERN_INFO "CDD: registered correctly with major number %d\n",
	  majorNumber);

  // Register the device class
  cddClass = class_create (THIS_MODULE, CLASS_NAME);
  if (IS_ERR (cddClass))
    {				// Check for error and clean up if there is
      unregister_chrdev (majorNumber, DEVICE_NAME);
      printk (KERN_ALERT "Failed to register device class\n");
      return PTR_ERR (cddClass);	// Correct way to return an error on a pointer
    }
  printk (KERN_INFO "CDD: device class registered correctly\n");

  // Register the device driver
  cddDevice =
    device_create (cddClass, NULL, MKDEV (majorNumber, 0), NULL, DEVICE_NAME);
  if (IS_ERR (cddDevice))
    {				// Clean up if there is an error
      class_destroy (cddClass);	// Repeated code but the alternative is goto statements
      unregister_chrdev (majorNumber, DEVICE_NAME);
      printk (KERN_ALERT "Failed to create the device\n");
      return PTR_ERR (cddDevice);
    }
  printk (KERN_INFO "CDD: device class created correctly\n");	// Made it! device was initialized
  return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit
cdd_exit (void)
{
  device_destroy (cddClass, MKDEV (majorNumber, 0));	// remove the device
  class_unregister (cddClass);	// unregister the device class
  class_destroy (cddClass);	// remove the device class
  unregister_chrdev (majorNumber, DEVICE_NAME);	// unregister the major number
  printk (KERN_INFO "CDD: Unregistered and de-initialized\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int
dev_open (struct inode *inodep, struct file *filep)
{
  printk (KERN_INFO "CDD: Opened");
  return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t
dev_read (struct file *filep, char *buffer, size_t len, loff_t * offset)
{
  int amountRead = 0;
  int i;
  for (i = 0; i < len; i++)
    {
      if (!isEmpty ())
	{
	  buffer[i] = get ();
	  amountRead++;
	}
      else
	{

	  break;
	}
    }

  printk (KERN_INFO "CDD: Sent %d characters to the user\n", amountRead);
  return amountRead;
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t
dev_write (struct file *filep, const char *buffer, size_t len,
	   loff_t * offset)
{
  int amountWritten = 0;
  int i;
  for (i = 0; i < len; i++)
    {
      amountWritten += add (buffer[i]);
    }

  printk (KERN_INFO "CDD: Received %zu characters from the user\n",
	  amountWritten);
  return amountWritten;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int
dev_release (struct inode *inodep, struct file *filep)
{
  printk (KERN_INFO "CDD: Device successfully closed\n");
  return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init (cdd_init);
module_exit (cdd_exit);
